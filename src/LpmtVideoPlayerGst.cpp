#ifdef WITH_HWDECODE

#include "LpmtVideoPlayerImpl.h"

#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <GL/glx.h>

#include <atomic>
#include <cstring>
#include <mutex>

namespace {

const char* PIPELINE_TEMPLATE =
    "filesrc name=src ! qtdemux name=demux ! queue ! h264parse ! vah264dec ! "
    "glupload ! glcolorconvert ! "
    "appsink name=sink emit-signals=true sync=true max-buffers=2 drop=false "
    "caps=video/x-raw(memory:GLMemory),format=RGBA,texture-target=%s";

const char* BLIT_VERT = R"(#version 120
varying vec2 vT;
void main() {
    vT = gl_MultiTexCoord0.xy;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)";

const char* BLIT_FRAG = R"(#version 120
uniform sampler2D src;
uniform int flipY;
varying vec2 vT;
void main() {
    vec2 uv = (flipY == 1) ? vec2(vT.x, 1.0 - vT.y) : vT;
    gl_FragColor = texture2D(src, uv);
}
)";

GstGLDisplay* gGlDisplay = nullptr;
GstGLContext* gGlContext = nullptr;

void ensureGstInit() {
    static std::once_flag flag;
    std::call_once(flag, []{ gst_init(nullptr, nullptr); });
}

void ensureGlBridge() {
    if (gGlDisplay && gGlContext) return;
    Display* xd = glXGetCurrentDisplay();
    GLXContext glx = glXGetCurrentContext();
    if (!xd || !glx) return;
    if (!gGlDisplay) {
        gGlDisplay = GST_GL_DISPLAY(gst_gl_display_x11_new_with_display(xd));
    }
    if (!gGlContext && gGlDisplay) {
        gGlContext = gst_gl_context_new_wrapped(gGlDisplay, (guintptr)glx,
            GST_GL_PLATFORM_GLX, GST_GL_API_OPENGL);
        ofLogNotice("LpmtVideoPlayer")
            << "GL bridge created: display=" << (void*)gGlDisplay
            << " context=" << (void*)gGlContext << " glx=" << (void*)glx;
    }
}

void onDemuxPadAdded(GstElement*, GstPad* pad, gpointer user) {
    GstCaps* c = gst_pad_get_current_caps(pad);
    if (!c) return;
    const GstStructure* s = gst_caps_get_structure(c, 0);
    const gchar* name = gst_structure_get_name(s);
    bool isAudio = g_str_has_prefix(name, "audio/");
    gst_caps_unref(c);
    if (!isAudio) return;

    GstElement* pipe = static_cast<GstElement*>(user);
    GstElement* fake = gst_element_factory_make("fakesink", nullptr);
    g_object_set(fake, "sync", TRUE, "async", FALSE, NULL);
    gst_bin_add(GST_BIN(pipe), fake);
    gst_element_sync_state_with_parent(fake);
    GstPad* sinkPad = gst_element_get_static_pad(fake, "sink");
    gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);
}

class GstHwImpl : public LpmtVideoPlayer::Impl {
public:
    ~GstHwImpl() override { teardown(); }

    void load(const std::string& path) override;
    void play() override;
    void stop() override;
    void close() override { teardown(); }
    void update() override;

    bool isLoaded() const override { return loaded; }
    bool isPaused() const override { return paused; }
    void setPaused(bool p) override;
    void setSpeed(float s) override;
    void setVolume(float) override {}
    void setLoopState(ofLoopType s) override { loopMode = s; }
    void setPosition(float pct) override;

    float getWidth()  const override { return (float)width;  }
    float getHeight() const override { return (float)height; }

    ofTexture& getTexture() override;
    void draw(float x, float y, float w, float h) override;

private:
    static GstBusSyncReply onBusSync(GstBus*, GstMessage*, gpointer);
    static void onNewSample(GstElement*, gpointer);

    bool tryBuild(const std::string& path, const char* target);
    void teardown();
    void seekSegment(bool flush);
    void configureRectTex();
    void configureBlit();

    GstElement*   pipeline  = nullptr;
    GstElement*   sink      = nullptr;
    GstVideoInfo  videoInfo{};
    bool loggedTarget = false;
    std::string   tag;

    enum class Mode { RectDirect, TwoDBlit };
    Mode mode = Mode::RectDirect;

    std::mutex heldMx;
    GstSample* held = nullptr;
    std::atomic<bool> newSample{false};

    ofTexture outTex;
    ofTexture srcTex2D;
    ofFbo     blitFbo;
    ofShader  blitShader;
    bool      blitReady = false;
    bool      firstSampleLogged = false;

    int width  = 0;
    int height = 0;
    double rate = 1.0;
    ofLoopType loopMode = OF_LOOP_NORMAL;
    bool loaded  = false;
    bool paused  = false;
    bool playing = false;
    bool needsFlipY = true;
    std::string sourcePath;
};

GstBusSyncReply GstHwImpl::onBusSync(GstBus*, GstMessage* msg, gpointer user) {
    if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_NEED_CONTEXT) return GST_BUS_PASS;
    auto* self = static_cast<GstHwImpl*>(user);
    const gchar* type = nullptr;
    gst_message_parse_context_type(msg, &type);
    GstElement* src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
    ofLogNotice("LpmtVideoPlayer")
        << "[" << self->tag << "] NEED_CONTEXT type=" << (type ? type : "?")
        << " src=" << GST_ELEMENT_NAME(src);
    if (g_strcmp0(type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0 && gGlDisplay) {
        GstContext* c = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display(c, gGlDisplay);
        gst_element_set_context(src, c);
        gst_context_unref(c);
    } else if (g_strcmp0(type, "gst.gl.app_context") == 0 && gGlContext) {
        GstContext* c = gst_context_new("gst.gl.app_context", TRUE);
        GstStructure* s = gst_context_writable_structure(c);
        gst_structure_set(s, "context", GST_TYPE_GL_CONTEXT, gGlContext, NULL);
        gst_element_set_context(src, c);
        gst_context_unref(c);
    }
    return GST_BUS_PASS;
}

void GstHwImpl::onNewSample(GstElement* el, gpointer user) {
    GstSample* s = gst_app_sink_pull_sample(GST_APP_SINK(el));
    if (!s) return;
    auto* self = static_cast<GstHwImpl*>(user);
    if (!self->firstSampleLogged) {
        self->firstSampleLogged = true;
        ofLogNotice("LpmtVideoPlayer") << "[" << self->tag << "] first new-sample delivered";
    }
    std::lock_guard<std::mutex> lk(self->heldMx);
    if (self->held) gst_sample_unref(self->held);
    self->held = s;
    self->newSample.store(true, std::memory_order_release);
}

bool GstHwImpl::tryBuild(const std::string& path, const char* target) {
    ofLogNotice("LpmtVideoPlayer") << "[" << tag << "] tryBuild target=" << target;
    gchar* desc = g_strdup_printf(PIPELINE_TEMPLATE, target);
    GError* err = nullptr;
    GstElement* p = gst_parse_launch(desc, &err);
    g_free(desc);
    if (err) { ofLogError("LpmtVideoPlayer") << "[" << tag << "] parse: " << err->message; g_error_free(err); }
    if (!p) return false;

    GstElement* src = gst_bin_get_by_name(GST_BIN(p), "src");
    g_object_set(src, "location", path.c_str(), NULL);
    gst_object_unref(src);

    GstElement* demux = gst_bin_get_by_name(GST_BIN(p), "demux");
    g_signal_connect(demux, "pad-added", G_CALLBACK(&onDemuxPadAdded), p);
    gst_object_unref(demux);

    sink = gst_bin_get_by_name(GST_BIN(p), "sink");
    g_signal_connect(sink, "new-sample", G_CALLBACK(&GstHwImpl::onNewSample), this);

    GstBus* bus = gst_element_get_bus(p);
    gst_bus_set_sync_handler(bus, &GstHwImpl::onBusSync, this, nullptr);
    gst_object_unref(bus);

    gst_element_set_state(p, GST_STATE_PAUSED);
    GstState st = GST_STATE_NULL, pend = GST_STATE_NULL;
    GstStateChangeReturn ret = gst_element_get_state(p, &st, &pend, 5 * GST_SECOND);
    const char* retStr = "?";
    switch (ret) {
        case GST_STATE_CHANGE_SUCCESS:  retStr = "SUCCESS"; break;
        case GST_STATE_CHANGE_ASYNC:    retStr = "ASYNC"; break;
        case GST_STATE_CHANGE_FAILURE:  retStr = "FAILURE"; break;
        case GST_STATE_CHANGE_NO_PREROLL: retStr = "NO_PREROLL"; break;
    }
    ofLogNotice("LpmtVideoPlayer")
        << "[" << tag << "] state change to PAUSED: " << retStr
        << " state=" << gst_element_state_get_name(st)
        << " pending=" << gst_element_state_get_name(pend);
    if (ret == GST_STATE_CHANGE_FAILURE || ret == GST_STATE_CHANGE_ASYNC) {
        gst_element_set_state(p, GST_STATE_NULL);
        gst_object_unref(p);
        sink = nullptr;
        return false;
    }

    GstPad* pad = gst_element_get_static_pad(sink, "sink");
    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (caps) {
        gchar* cs = gst_caps_to_string(caps);
        ofLogNotice("LpmtVideoPlayer") << "[" << tag << "] sink caps: " << (cs ? cs : "?");
        g_free(cs);
        if (gst_video_info_from_caps(&videoInfo, caps)) {
            width  = videoInfo.width;
            height = videoInfo.height;
        }
        gst_caps_unref(caps);
    } else {
        ofLogWarning("LpmtVideoPlayer") << "[" << tag << "] sink has no caps after PAUSED";
    }
    gst_object_unref(pad);

    pipeline = p;
    return width > 0 && height > 0;
}

void GstHwImpl::configureRectTex() {
    ofTextureData td;
    td.width  = width;
    td.height = height;
    td.tex_w  = width;
    td.tex_h  = height;
    td.tex_t  = width;
    td.tex_u  = height;
    td.textureTarget    = GL_TEXTURE_RECTANGLE_ARB;
    td.glInternalFormat = GL_RGBA;
    outTex.allocate(td);
}

void GstHwImpl::configureBlit() {
    ofTextureData td;
    td.width  = width;
    td.height = height;
    td.tex_w  = width;
    td.tex_h  = height;
    td.tex_t  = 1.0f;
    td.tex_u  = 1.0f;
    td.textureTarget    = GL_TEXTURE_2D;
    td.glInternalFormat = GL_RGBA;
    srcTex2D.allocate(td);

    ofFbo::Settings s;
    s.width          = width;
    s.height         = height;
    s.internalformat = GL_RGBA;
    s.textureTarget  = GL_TEXTURE_RECTANGLE_ARB;
    s.useDepth       = false;
    blitFbo.allocate(s);

    blitShader.setupShaderFromSource(GL_VERTEX_SHADER,   BLIT_VERT);
    blitShader.setupShaderFromSource(GL_FRAGMENT_SHADER, BLIT_FRAG);
    blitShader.linkProgram();
    blitReady = true;
}

void GstHwImpl::load(const std::string& path) {
    teardown();
    ensureGstInit();
    ensureGlBridge();
    if (!gGlDisplay || !gGlContext) {
        ofLogError("LpmtVideoPlayer") << "no GL bridge (call load from main GL thread)";
        return;
    }

    {
        const char* slash = strrchr(path.c_str(), '/');
        tag = slash ? (slash + 1) : path;
        if (tag.size() > 32) tag = tag.substr(0, 32);
    }
    sourcePath = path;
    ofLogNotice("LpmtVideoPlayer") << "[" << tag << "] load begin path=" << path;

    if (tryBuild(path, "rectangle")) {
        mode = Mode::RectDirect;
        configureRectTex();
        ofLogNotice("LpmtVideoPlayer")
            << "[" << tag << "] hw decode rectangle-direct " << width << "x" << height;
    } else if (tryBuild(path, "2D")) {
        mode = Mode::TwoDBlit;
        configureBlit();
        ofLogNotice("LpmtVideoPlayer")
            << "[" << tag << "] hw decode 2D+blit " << width << "x" << height;
    } else {
        ofLogError("LpmtVideoPlayer") << "[" << tag << "] pipeline build failed";
        return;
    }
    loaded  = true;
    paused  = false;
    playing = false;
}

void GstHwImpl::teardown() {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
        sink = nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(heldMx);
        if (held) { gst_sample_unref(held); held = nullptr; }
        newSample.store(false, std::memory_order_release);
    }
    loaded = false; playing = false; paused = false;
    width = height = 0;
    blitReady = false;
    loggedTarget = false;
}

void GstHwImpl::seekSegment(bool flush) {
    if (!pipeline) return;
    GstSeekFlags flags = GstSeekFlags(GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_KEY_UNIT);
    if (flush) flags = GstSeekFlags(flags | GST_SEEK_FLAG_FLUSH);
    gst_element_seek(pipeline, rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, 0,
        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
}

void GstHwImpl::play() {
    if (!pipeline) return;
    if (loopMode == OF_LOOP_NORMAL) {
        seekSegment(true);
    } else {
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
            GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0);
    }
    GstStateChangeReturn r = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    const char* rs = "?";
    switch (r) {
        case GST_STATE_CHANGE_SUCCESS:  rs = "SUCCESS"; break;
        case GST_STATE_CHANGE_ASYNC:    rs = "ASYNC"; break;
        case GST_STATE_CHANGE_FAILURE:  rs = "FAILURE"; break;
        case GST_STATE_CHANGE_NO_PREROLL: rs = "NO_PREROLL"; break;
    }
    ofLogNotice("LpmtVideoPlayer") << "[" << tag << "] play -> PLAYING: " << rs;
    playing = true;
    paused  = false;
}

void GstHwImpl::stop() {
    if (!pipeline) return;
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    playing = false;
}

void GstHwImpl::setPaused(bool p) {
    if (!pipeline) return;
    gst_element_set_state(pipeline, p ? GST_STATE_PAUSED : GST_STATE_PLAYING);
    paused = p;
}

void GstHwImpl::setSpeed(float s) {
    if (s == 0.0f) return;
    rate = s;
    if (pipeline && loopMode == OF_LOOP_NORMAL) seekSegment(true);
}

void GstHwImpl::setPosition(float pct) {
    if (!pipeline) return;
    gint64 dur = GST_CLOCK_TIME_NONE;
    if (!gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur) || dur <= 0) return;
    gint64 pos = (gint64)((double)pct * (double)dur);
    gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), pos);
}

void GstHwImpl::update() {
    if (!pipeline) return;
    GstBus* bus = gst_element_get_bus(pipeline);
    GstMessage* msg = nullptr;
    while ((msg = gst_bus_pop_filtered(bus,
             GstMessageType(GST_MESSAGE_SEGMENT_DONE | GST_MESSAGE_EOS
                          | GST_MESSAGE_ERROR | GST_MESSAGE_WARNING))) != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_SEGMENT_DONE:
            if (loopMode == OF_LOOP_NORMAL) seekSegment(false);
            break;
        case GST_MESSAGE_EOS:
            if (loopMode == OF_LOOP_NORMAL) seekSegment(true);
            else playing = false;
            break;
        case GST_MESSAGE_ERROR: {
            GError* e = nullptr; gchar* d = nullptr;
            gst_message_parse_error(msg, &e, &d);
            ofLogError("LpmtVideoPlayer") << "[" << tag << "] ERROR: "
                << (e ? e->message : "?") << " | " << (d ? d : "");
            if (e) g_error_free(e);
            g_free(d);
            playing = false;
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* e = nullptr; gchar* d = nullptr;
            gst_message_parse_warning(msg, &e, &d);
            ofLogWarning("LpmtVideoPlayer") << "[" << tag << "] WARN: "
                << (e ? e->message : "?") << " | " << (d ? d : "");
            if (e) g_error_free(e);
            g_free(d);
            break;
        }
        default: break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

ofTexture& GstHwImpl::getTexture() {
    if (newSample.exchange(false, std::memory_order_acq_rel)) {
        GstSample* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(heldMx);
            if (held) { s = held; gst_sample_ref(s); }
        }
        if (s) {
            GstBuffer* buf = gst_sample_get_buffer(s);
            GstVideoFrame frame;
            if (gst_video_frame_map(&frame, &videoInfo, buf,
                    (GstMapFlags)(GST_MAP_READ | GST_MAP_GL))) {
                GLuint id = *(GLuint*)frame.data[0];
                if (!loggedTarget) {
                    GstMemory* mem = gst_buffer_peek_memory(buf, 0);
                    GstGLTextureTarget t = GST_GL_TEXTURE_TARGET_NONE;
                    if (mem && gst_is_gl_memory(mem)) {
                        t = gst_gl_memory_get_texture_target((GstGLMemory*)mem);
                    }
                    ofLogNotice("LpmtVideoPlayer")
                        << "[" << tag << "] first sample: id=" << id
                        << " target=" << gst_gl_texture_target_to_string(t)
                        << " " << width << "x" << height;
                    loggedTarget = true;
                }
                if (mode == Mode::RectDirect) {
                    outTex.setUseExternalTextureID(id);
                } else if (blitReady) {
                    srcTex2D.setUseExternalTextureID(id);
                    blitFbo.begin();
                    ofClear(0);
                    blitShader.begin();
                    blitShader.setUniformTexture("src", srcTex2D, 0);
                    blitShader.setUniform1i("flipY", needsFlipY ? 1 : 0);
                    ofDrawRectangle(0, 0, (float)width, (float)height);
                    blitShader.end();
                    blitFbo.end();
                }
                gst_video_frame_unmap(&frame);
            }
            gst_sample_unref(s);
        }
    }
    return (mode == Mode::RectDirect) ? outTex : blitFbo.getTexture();
}

void GstHwImpl::draw(float x, float y, float w, float h) {
    getTexture().draw(x, y, w, h);
}

}

std::shared_ptr<LpmtVideoPlayer::Impl> makeLpmtVideoPlayerImpl() {
    return std::make_shared<GstHwImpl>();
}

#endif
