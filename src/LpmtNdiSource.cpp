#include "LpmtNdiSource.h"
#include "ofxNDIreceiver.h"

struct LpmtNdiSource::Impl {
    ofxNDIreceiver receiver;
    ofTexture texture;
    std::string name;
};

namespace {

ofTexture& emptyTexture()
{
    static ofTexture texture;
    return texture;
}

// constructed on first use so the NDI runtime is loaded after the GL context exists
ofxNDIreceiver& finder()
{
    static ofxNDIreceiver receiver;
    return receiver;
}

} // namespace

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
    impl->receiver.SetSenderName(name);
}

const std::string& LpmtNdiSource::source() const
{
    static const std::string none;
    return impl ? impl->name : none;
}

void LpmtNdiSource::update()
{
    if (!impl || impl->name.empty()) return;

    // the UYVY path leaves alpha blending disabled behind it
    ofPushStyle();
    impl->receiver.ReceiveImage(impl->texture);
    ofPopStyle();
}

void LpmtNdiSource::close()
{
    if (!impl) return;
    impl->receiver.ReleaseReceiver();
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
    finder().FindSenders();
    return finder().GetSenderList();
}
