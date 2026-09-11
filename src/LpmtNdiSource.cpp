#include "LpmtNdiSource.h"
#include "ofxNDIdynloader.h"

namespace {

// loaded on first use so the runtime comes up after the GL context exists
const NDIlib_v5* ndiLib()
{
    static ofxNDIdynloader loader;
    static const NDIlib_v5* lib = loader.Load();
    return lib;
}

NDIlib_find_instance_t ndiFinder()
{
    static NDIlib_find_instance_t finder = [] {
        const NDIlib_v5* lib = ndiLib();
        if (!lib) return NDIlib_find_instance_t(nullptr);
        NDIlib_find_create_t desc{};
        desc.show_local_sources = true;
        desc.p_groups = nullptr;
        desc.p_extra_ips = nullptr;
        return lib->find_create_v2(&desc);
    }();
    return finder;
}

ofTexture& emptyTexture()
{
    static ofTexture texture;
    return texture;
}

} // namespace

struct LpmtNdiSource::Impl {
    NDIlib_recv_instance_t receiver = nullptr;
    NDIlib_framesync_instance_t sync = nullptr;
    ofTexture texture;
    std::string name;
    int64_t lastTimestamp = 0;
    NDIlib_FourCC_video_type_e lastFourCC = NDIlib_FourCC_video_type_max;

    ~Impl() { disconnect(); }

    void connect(const std::string& sender)
    {
        disconnect();
        const NDIlib_v5* lib = ndiLib();
        if (!lib) return;

        // name alone resolves through discovery, the finder just has the url already
        NDIlib_source_t source{};
        source.p_ndi_name = sender.c_str();
        source.p_url_address = nullptr;
        if (NDIlib_find_instance_t finder = ndiFinder()) {
            uint32_t count = 0;
            const NDIlib_source_t* sources = lib->find_get_current_sources(finder, &count);
            for (uint32_t i = 0; i < count; i++) {
                if (sender == sources[i].p_ndi_name) {
                    source = sources[i];
                    break;
                }
            }
        }

        NDIlib_recv_create_v3_t desc{};
        desc.source_to_connect_to = source;
        desc.color_format = NDIlib_recv_color_format_RGBX_RGBA;
        desc.bandwidth = NDIlib_recv_bandwidth_highest;
        desc.allow_video_fields = false;
        desc.p_ndi_recv_name = "LPMT";
        receiver = lib->recv_create_v3(&desc);
        if (receiver) {
            sync = lib->framesync_create(receiver);
        }
    }

    void disconnect()
    {
        const NDIlib_v5* lib = ndiLib();
        if (lib && sync) lib->framesync_destroy(sync);
        if (lib && receiver) lib->recv_destroy(receiver);
        sync = nullptr;
        receiver = nullptr;
        lastTimestamp = 0;
        lastFourCC = NDIlib_FourCC_video_type_max;
    }

    void upload(const NDIlib_video_frame_v2_t& frame)
    {
        if (!texture.isAllocated()
            || (int)texture.getWidth() != frame.xres
            || (int)texture.getHeight() != frame.yres) {
            texture.allocate(frame.xres, frame.yres, GL_RGBA);
            lastFourCC = NDIlib_FourCC_video_type_max;
        }
        if (frame.FourCC != lastFourCC) {
            // RGBX leaves the fourth byte undefined
            const bool hasAlpha = (frame.FourCC == NDIlib_FourCC_video_type_RGBA);
            texture.setSwizzle(GL_TEXTURE_SWIZZLE_A, hasAlpha ? GL_ALPHA : GL_ONE);
            lastFourCC = frame.FourCC;
        }

        const int rowLength = frame.line_stride_in_bytes / 4;
        const bool padded = (rowLength != frame.xres);
        if (padded) glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
        texture.loadData(frame.p_data, frame.xres, frame.yres, GL_RGBA);
        if (padded) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
};

LpmtNdiSource::LpmtNdiSource() = default;
LpmtNdiSource::~LpmtNdiSource() = default;

LpmtNdiSource::LpmtNdiSource(const LpmtNdiSource& other)
{
    setSource(other.source());
}

LpmtNdiSource& LpmtNdiSource::operator=(const LpmtNdiSource& other)
{
    if (this != &other) {
        impl.reset();
        setSource(other.source());
    }
    return *this;
}

void LpmtNdiSource::setSource(const std::string& name)
{
    if (name.empty()) {
        close();
        return;
    }
    if (!impl) {
        impl = std::make_shared<Impl>();
    }
    if (impl->name == name) return;

    impl->name = name;
    impl->connect(name);
}

const std::string& LpmtNdiSource::source() const
{
    static const std::string none;
    return impl ? impl->name : none;
}

void LpmtNdiSource::update()
{
    if (!impl || !impl->sync) return;
    const NDIlib_v5* lib = ndiLib();

    // the frame sync paces the stream to our draw rate instead of handing over
    // whatever packet arrived last, which is what made playback stutter
    NDIlib_video_frame_v2_t frame{};
    lib->framesync_capture_video(impl->sync, &frame, NDIlib_frame_format_type_progressive);
    if (!frame.p_data) return;

    if (frame.xres > 0 && frame.yres > 0 && frame.timestamp != impl->lastTimestamp) {
        impl->upload(frame);
        impl->lastTimestamp = frame.timestamp;
    }
    lib->framesync_free_video(impl->sync, &frame);
}

void LpmtNdiSource::close()
{
    if (!impl) return;
    impl->disconnect();
    impl->texture.clear();
    impl->name.clear();
}

bool LpmtNdiSource::isReady() const
{
    return impl && impl->texture.isAllocated() && (impl->texture.getWidth() > 0);
}

float LpmtNdiSource::getWidth() const
{
    return impl ? impl->texture.getWidth() : 0.0f;
}

float LpmtNdiSource::getHeight() const
{
    return impl ? impl->texture.getHeight() : 0.0f;
}

ofTexture& LpmtNdiSource::getTexture()
{
    return impl ? impl->texture : emptyTexture();
}

void LpmtNdiSource::draw(float x, float y, float w, float h)
{
    if (isReady()) {
        impl->texture.draw(x, y, w, h);
    }
}

std::vector<std::string> LpmtNdi::findSources()
{
    std::vector<std::string> names;
    const NDIlib_v5* lib = ndiLib();
    NDIlib_find_instance_t finder = ndiFinder();
    if (!lib || !finder) return names;

    uint32_t count = 0;
    const NDIlib_source_t* sources = lib->find_get_current_sources(finder, &count);
    for (uint32_t i = 0; i < count; i++) {
        names.push_back(sources[i].p_ndi_name);
    }
    return names;
}
