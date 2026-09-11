#include "ofApp.h"

#include <functional>
#include <map>

// OSC surface control is addressed as /surface/<n>/<parameter>, where <n> is the
// surface index. There is deliberately no notion of a "currently selected" surface
// here - every message carries its own target, so senders never have to track state.
//
// For a boolean parameter, an argument sets it and no argument toggles it.

namespace {

using SurfaceFn = std::function<void(ofApp&, quad&, const ofxOscMessage&)>;
using GlobalFn = std::function<void(ofApp&, const ofxOscMessage&)>;

bool hasArg(const ofxOscMessage& m, std::size_t i)
{
    return i < m.getNumArgs();
}

float argFloat(const ofxOscMessage& m, std::size_t i, float fallback = 0.0f)
{
    if (!hasArg(m, i)) return fallback;
    switch (m.getArgType(i)) {
    case OFXOSC_TYPE_FLOAT: return m.getArgAsFloat(i);
    case OFXOSC_TYPE_DOUBLE: return (float)m.getArgAsDouble(i);
    case OFXOSC_TYPE_INT32: return (float)m.getArgAsInt32(i);
    case OFXOSC_TYPE_INT64: return (float)m.getArgAsInt64(i);
    case OFXOSC_TYPE_TRUE: return 1.0f;
    case OFXOSC_TYPE_FALSE: return 0.0f;
    case OFXOSC_TYPE_STRING: return ofToFloat(m.getArgAsString(i));
    default: return fallback;
    }
}

int argInt(const ofxOscMessage& m, std::size_t i, int fallback = 0)
{
    if (!hasArg(m, i)) return fallback;
    switch (m.getArgType(i)) {
    case OFXOSC_TYPE_INT32: return m.getArgAsInt32(i);
    case OFXOSC_TYPE_INT64: return (int)m.getArgAsInt64(i);
    case OFXOSC_TYPE_FLOAT: return (int)m.getArgAsFloat(i);
    case OFXOSC_TYPE_DOUBLE: return (int)m.getArgAsDouble(i);
    case OFXOSC_TYPE_TRUE: return 1;
    case OFXOSC_TYPE_FALSE: return 0;
    case OFXOSC_TYPE_STRING: return ofToInt(m.getArgAsString(i));
    default: return fallback;
    }
}

std::string argString(const ofxOscMessage& m, std::size_t i, const std::string& fallback = "")
{
    if (!hasArg(m, i)) return fallback;
    if (m.getArgType(i) == OFXOSC_TYPE_STRING || m.getArgType(i) == OFXOSC_TYPE_SYMBOL) {
        return m.getArgAsString(i);
    }
    return fallback;
}

void addBool(std::map<std::string, SurfaceFn>& table, const std::string& path, bool quad::*member, bool withShowAlias = false)
{
    const SurfaceFn fn = [member](ofApp&, quad& q, const ofxOscMessage& m) {
        if (m.getNumArgs() == 0) q.*member = !(q.*member);
        else q.*member = (argFloat(m, 0) != 0.0f);
    };
    table[path] = fn;
    if (withShowAlias) table[path + "/show"] = fn;
}

void addFloat(std::map<std::string, SurfaceFn>& table, const std::string& path, float quad::*member, float lo, float hi)
{
    table[path] = [member, lo, hi](ofApp&, quad& q, const ofxOscMessage& m) {
        if (!hasArg(m, 0)) return;
        q.*member = ofClamp(argFloat(m, 0), lo, hi);
    };
}

void addInt(std::map<std::string, SurfaceFn>& table, const std::string& path, int quad::*member, int lo, int hi)
{
    table[path] = [member, lo, hi](ofApp&, quad& q, const ofxOscMessage& m) {
        if (!hasArg(m, 0)) return;
        q.*member = (int)ofClamp((float)argInt(m, 0), (float)lo, (float)hi);
    };
}

void addFloatArray(std::map<std::string, SurfaceFn>& table, const std::string& path, float (quad::*member)[4], int count, const char* const* names)
{
    for (int i = 0; i < count; i++) {
        table[path + "/" + names[i]] = [member, i](ofApp&, quad& q, const ofxOscMessage& m) {
            if (!hasArg(m, 0)) return;
            (q.*member)[i] = ofClamp(argFloat(m, 0), 0.0f, 1.0f);
        };
    }
}

void addColor(std::map<std::string, SurfaceFn>& table, const std::string& path, ofFloatColor quad::*member)
{
    // all four components at once, or one at a time as /1 (r) through /4 (a)
    table[path] = [member](ofApp&, quad& q, const ofxOscMessage& m) {
        ofFloatColor& c = q.*member;
        c.r = ofClamp(argFloat(m, 0, c.r), 0.0f, 1.0f);
        c.g = ofClamp(argFloat(m, 1, c.g), 0.0f, 1.0f);
        c.b = ofClamp(argFloat(m, 2, c.b), 0.0f, 1.0f);
        c.a = ofClamp(argFloat(m, 3, c.a), 0.0f, 1.0f);
    };
    for (int i = 0; i < 4; i++) {
        table[path + "/" + ofToString(i + 1)] = [member, i](ofApp&, quad& q, const ofxOscMessage& m) {
            if (!hasArg(m, 0)) return;
            ofFloatColor& c = q.*member;
            (&c.r)[i] = ofClamp(argFloat(m, 0), 0.0f, 1.0f);
        };
    }
}

const char* const rectCropNames[4] = { "top", "right", "bottom", "left" };
const char* const circCropNames[3] = { "x", "y", "radius" };

std::map<std::string, SurfaceFn> buildSurfaceTable()
{
    std::map<std::string, SurfaceFn> t;

    addBool(t, "show", &quad::isOn);

    addBool(t, "timeline/tint", &quad::bTimelineTint);
    addBool(t, "timeline/color", &quad::bTimelineColor);
    addBool(t, "timeline/alpha", &quad::bTimelineAlpha);
    addBool(t, "timeline/slides", &quad::bTimelineSlideChange);

    addBool(t, "img", &quad::imgBg, true);
    addBool(t, "img/hmirror", &quad::imgHFlip);
    addBool(t, "img/vmirror", &quad::imgVFlip);
    addInt(t, "img/rotation", &quad::imgRotation, 0, 3);
    addBool(t, "img/center", &quad::imgCenter);
    addBool(t, "img/fit", &quad::imageFit);
    addBool(t, "img/keepaspect", &quad::imageKeepAspect);
    addFloat(t, "img/mult/x", &quad::imgMultX, 0.1f, 5.0f);
    addFloat(t, "img/mult/y", &quad::imgMultY, 0.1f, 5.0f);
    addColor(t, "img/color", &quad::imgColorize);

    addFloat(t, "hue", &quad::hue, 0.0f, 1.0f);
    addFloat(t, "saturation", &quad::saturation, 0.0f, 1.0f);
    addFloat(t, "luminance", &quad::luminance, 0.0f, 1.0f);

    addBool(t, "blendmodes", &quad::bBlendModes, true);
    addInt(t, "blendmodes/mode", &quad::blendMode, 0, 5);

    addBool(t, "solid", &quad::colorBg, true);

    addBool(t, "mask", &quad::bMask, true);
    addBool(t, "mask/invert", &quad::maskInvert);
    addBool(t, "mask/outline", &quad::bDrawMaskOutline);

    addBool(t, "deform/bezier", &quad::bBezier);
    addBool(t, "deform/grid", &quad::bGrid);
    addInt(t, "deform/grid/rows", &quad::gridRows, 2, 15);
    addInt(t, "deform/grid/columns", &quad::gridColumns, 2, 20);

    addBool(t, "edgeblend", &quad::bEdgeBlend, true);
    addFloat(t, "edgeblend/power", &quad::edgeBlendExponent, 0.0f, 4.0f);
    addFloat(t, "edgeblend/gamma", &quad::edgeBlendGamma, 0.0f, 4.0f);
    addFloat(t, "edgeblend/luminance", &quad::edgeBlendLuminance, -4.0f, 4.0f);
    addFloat(t, "edgeblend/amount/left", &quad::edgeBlendAmountSin, 0.0f, 0.5f);
    addFloat(t, "edgeblend/amount/right", &quad::edgeBlendAmountDx, 0.0f, 0.5f);
    addFloat(t, "edgeblend/amount/top", &quad::edgeBlendAmountTop, 0.0f, 0.5f);
    addFloat(t, "edgeblend/amount/bottom", &quad::edgeBlendAmountBottom, 0.0f, 0.5f);
    t["edgeblend/amount"] = [](ofApp&, quad& q, const ofxOscMessage& m) {
        q.edgeBlendAmountSin = ofClamp(argFloat(m, 0, q.edgeBlendAmountSin), 0.0f, 1.0f);
        q.edgeBlendAmountDx = ofClamp(argFloat(m, 1, q.edgeBlendAmountDx), 0.0f, 1.0f);
        q.edgeBlendAmountTop = ofClamp(argFloat(m, 2, q.edgeBlendAmountTop), 0.0f, 1.0f);
        q.edgeBlendAmountBottom = ofClamp(argFloat(m, 3, q.edgeBlendAmountBottom), 0.0f, 1.0f);
    };

    addInt(t, "placement/x", &quad::quadDispX, -1600, 1600);
    addInt(t, "placement/y", &quad::quadDispY, -1600, 1600);
    addInt(t, "placement/w", &quad::quadW, 0, 2400);
    addInt(t, "placement/h", &quad::quadH, 0, 2400);
    t["placement"] = [](ofApp&, quad& q, const ofxOscMessage& m) {
        q.quadDispX = argInt(m, 0, q.quadDispX);
        q.quadDispY = argInt(m, 1, q.quadDispY);
    };
    t["placement/dimensions"] = [](ofApp&, quad& q, const ofxOscMessage& m) {
        q.quadW = argInt(m, 0, q.quadW);
        q.quadH = argInt(m, 1, q.quadH);
    };

    addBool(t, "video", &quad::videoBg, true);
    addFloat(t, "video/speed", &quad::videoSpeed, -2.0f, 4.0f);
    addFloat(t, "video/volume", &quad::videoVolume, 0.0f, 1.0f);
    addBool(t, "video/loop", &quad::videoLoop);

    addBool(t, "sharedvideo", &quad::sharedVideoBg, true);
    addInt(t, "sharedvideo/num", &quad::sharedVideoNum, 1, MAX_SHARED_VIDEOS);
    addBool(t, "sharedvideo/tiling", &quad::sharedVideoTiling);

    addBool(t, "cam", &quad::camBg, true);

    addBool(t, "ndi", &quad::ndiBg, true);

    addBool(t, "greenscreen", &quad::bUseGreenscreen, true);
    addFloat(t, "greenscreen/threshold", &quad::thresholdGreenscreen, 0.0f, 255.0f);
    addColor(t, "greenscreen/color", &quad::colorGreenscreen);

    addBool(t, "slideshow", &quad::slideshowBg, true);
    addInt(t, "slideshow/folder", &quad::bgSlideshow, 0, 64);
    addFloat(t, "slideshow/duration", &quad::slideshowSpeed, 0.1f, 15.0f);
    addBool(t, "slideshow/transitions", &quad::bFadeTransitions);

    addFloatArray(t, "crop/rectangular", &quad::crop, 4, rectCropNames);
    for (int i = 0; i < 3; i++) {
        const float hi = (i == 2) ? 2.0f : 1.0f; // radius goes further than the centre coordinates
        t[std::string("crop/circular/") + circCropNames[i]] = [i, hi](ofApp&, quad& q, const ofxOscMessage& m) {
            if (!hasArg(m, 0)) return;
            q.circularCrop[i] = ofClamp(argFloat(m, 0), 0.0f, hi);
        };
    }

    for (int c = 0; c < 4; c++) {
        const std::string base = "corners/" + ofToString(c);
        t[base] = [c](ofApp&, quad& q, const ofxOscMessage& m) {
            q.corners[c].x = argFloat(m, 0, q.corners[c].x);
            q.corners[c].y = argFloat(m, 1, q.corners[c].y);
        };
        t[base + "/x"] = [c](ofApp&, quad& q, const ofxOscMessage& m) {
            if (hasArg(m, 0)) q.corners[c].x = argFloat(m, 0);
        };
        t[base + "/y"] = [c](ofApp&, quad& q, const ofxOscMessage& m) {
            if (hasArg(m, 0)) q.corners[c].y = argFloat(m, 0);
        };
    }

#ifdef WITH_KINECT
    addBool(t, "kinect", &quad::kinectBg, true);
    addBool(t, "kinect/show/image", &quad::kinectImg);
    addBool(t, "kinect/show/grayscale", &quad::getKinectGrayImage);
    addBool(t, "kinect/mask", &quad::kinectMask);
    addFloat(t, "kinect/scale/x", &quad::kinectMultX, 0.1f, 10.0f);
    addFloat(t, "kinect/scale/y", &quad::kinectMultY, 0.1f, 10.0f);
    addInt(t, "kinect/threshold/near", &quad::nearDepthTh, 0, 255);
    addInt(t, "kinect/threshold/far", &quad::farDepthTh, 0, 255);
    addInt(t, "kinect/blur", &quad::kinectBlur, 0, 10);
    addBool(t, "kinect/contour", &quad::getKinectContours);
    addBool(t, "kinect/contour/curves", &quad::kinectContourCurved);
    addInt(t, "kinect/contour/smooth", &quad::kinectContourSmooth, 0, 20);
    addFloat(t, "kinect/contour/simplify", &quad::kinectContourSimplify, 0.0f, 2.0f);
    addFloat(t, "kinect/contour/area/min", &quad::kinectContourMin, 0.01f, 1.0f);
    addFloat(t, "kinect/contour/area/max", &quad::kinectContourMax, 0.0f, 1.0f);
    addColor(t, "kinect/color", &quad::kinectColorize);
#endif

    return t;
}

// surface handlers that need the owning app, not just the quad
std::map<std::string, SurfaceFn> buildSurfaceActionTable()
{
    std::map<std::string, SurfaceFn> t;

    t["img/load"] = [](ofApp& app, quad&, const ofxOscMessage&) { app.openImageFile(); };
    t["video/load"] = [](ofApp& app, quad&, const ofxOscMessage&) { app.openVideoFile(); };
    t["slideshow/load"] = [](ofApp& app, quad&, const ofxOscMessage&) { app.loadSlideshow(); };

    t["img/path"] = [](ofApp&, quad& q, const ofxOscMessage& m) {
        const std::string path = argString(m, 0);
        if (path == "") return;
        q.loadImageFromFile(ofFilePath::getFileName(path), path);
        q.imgBg = true;
    };
    t["video/path"] = [](ofApp&, quad& q, const ofxOscMessage& m) {
        const std::string path = argString(m, 0);
        if (path == "") return;
        q.loadVideoFromFile(ofFilePath::getFileName(path), path);
        q.videoBg = true;
    };
    t["img/blob"] = [](ofApp& app, quad& q, const ofxOscMessage& m) {
        if (argInt(m, 0) != app.appId) return;
        if (!hasArg(m, 1) || m.getArgType(1) != OFXOSC_TYPE_BLOB) return;
        ofBuffer buffer = m.getArgAsBlob(1);
        q.img.load(buffer);
        q.imgBg = true;
    };

    t["ndi/source"] = [](ofApp& app, quad& q, const ofxOscMessage& m) {
        const std::string name = argString(m, 0);
        q.ndiSourceName = name;
        q.ndiSourceIndex = app.ndiChoiceForSource(name);
        if (name.empty()) q.ndi.close();
    };
    t["ndi/select"] = [](ofApp& app, quad& q, const ofxOscMessage& m) {
        const int source = argInt(m, 0, -1);
        if ((source < 0) || (source >= (int)app.m_ndiSources.size())) return;
        q.ndiSourceName = app.m_ndiSources[source];
        q.ndiSourceIndex = source + 1;
    };

    t["cam/select"] = [](ofApp& app, quad& q, const ofxOscMessage& m) {
        const int cam = argInt(m, 0, q.camNumber);
        if ((cam >= 0) && (cam < (int)app.m_cameras.size())) q.camNumber = cam;
    };

    return t;
}

const std::map<std::string, GlobalFn>& globalTable()
{
    static const std::map<std::string, GlobalFn> t = {
        { "/projection/resync", [](ofApp& app, const ofxOscMessage&) { app.resync(); } },
        { "/projection/start", [](ofApp& app, const ofxOscMessage&) { app.startProjection(); } },
        { "/projection/stop", [](ofApp& app, const ofxOscMessage&) { app.stopProjection(); } },

        { "/projection/save", [](ofApp& app, const ofxOscMessage& m) {
             app.saveCurrentSettingsToXMLFile(argString(m, 0, DEFAULT_PROJECT_FILE));
         } },
        { "/projection/load", [](ofApp& app, const ofxOscMessage& m) {
             app.loadSettingsFromXMLFile(argString(m, 0, DEFAULT_PROJECT_FILE));
         } },

        { "/projection/fullscreen", [](ofApp& app, const ofxOscMessage& m) {
             app.setFullscreen(m.getNumArgs() == 0 ? !app.bFullscreen : (argFloat(m, 0) != 0.0f));
         } },
        { "/projection/gui", [](ofApp& app, const ofxOscMessage& m) {
             const bool wanted = (m.getNumArgs() == 0) ? !app.bGui : (argFloat(m, 0) != 0.0f);
             if (wanted != app.bGui) {
                 app.m_gui.toggleDraw();
                 app.bGui = wanted;
             }
         } },
        { "/projection/mode/setup", [](ofApp& app, const ofxOscMessage& m) {
             app.setEditMode(m.getNumArgs() == 0 ? !app.isEditMode : (argFloat(m, 0) != 0.0f));
         } },
        { "/projection/mode/masksetup", [](ofApp& app, const ofxOscMessage& m) {
             app.setMaskSetup(m.getNumArgs() == 0 ? !app.maskSetup : (argFloat(m, 0) != 0.0f));
         } },

        { "/projection/timeline/use", [](ofApp& app, const ofxOscMessage& m) {
             app.useTimeline = (m.getNumArgs() == 0) ? !app.useTimeline : (argFloat(m, 0) != 0.0f);
         } },
        { "/projection/timeline/duration", [](ofApp& app, const ofxOscMessage& m) {
             const float seconds = argFloat(m, 0);
             if (seconds >= 10.0f) app.timelineDurationSeconds = seconds;
         } },
        { "/projection/timeline/play", [](ofApp& app, const ofxOscMessage&) { app.timeline.togglePlay(); } },
        { "/projection/timeline/show", [](ofApp& app, const ofxOscMessage& m) {
             app.setTimelineVisible(m.getNumArgs() == 0 ? !app.bTimeline : (argFloat(m, 0) != 0.0f));
         } },

        { "/projection/ndi/refresh", [](ofApp& app, const ofxOscMessage&) { app.refreshNdiSources(); } },

        { "/projection/sharedvideo/path", [](ofApp& app, const ofxOscMessage& m) {
             const int slot = argInt(m, 0, -1);
             const std::string path = argString(m, 1);
             if ((slot >= 1) && (slot <= MAX_SHARED_VIDEOS) && (path != "")) {
                 app.openSharedVideoFile(path, slot - 1);
             }
         } },
    };
    return t;
}

} // namespace

//--------------------------------------------------------------
void ofApp::parseOsc()
{
    ofxOscMessage m;
    receiver.getNextMessage(m);

    const std::string address = m.getAddress();

    const auto global = globalTable().find(address);
    if (global != globalTable().end()) {
        global->second(*this, m);
        return;
    }

    static const std::map<std::string, SurfaceFn> surfaceTable = buildSurfaceTable();
    static const std::map<std::string, SurfaceFn> surfaceActions = buildSurfaceActionTable();

    // /surface/<n>/<parameter>
    const std::string prefix = "/surface/";
    if (address.compare(0, prefix.size(), prefix) != 0) {
        ofLogVerbose("LPMT") << "unhandled OSC address " << address;
        return;
    }

    const std::size_t indexEnd = address.find('/', prefix.size());
    if (indexEnd == std::string::npos) return;

    const std::string indexText = address.substr(prefix.size(), indexEnd - prefix.size());
    if (indexText.empty() || (indexText.find_first_not_of("0123456789") != std::string::npos)) {
        ofLogWarning("LPMT") << "OSC address " << address << " has no valid surface index";
        return;
    }

    const int surface = ofToInt(indexText);
    if ((surface < 0) || (surface >= MAX_QUADS) || !quads[surface].initialized) {
        ofLogWarning("LPMT") << "OSC message for surface " << surface << " which does not exist";
        return;
    }

    const std::string parameter = address.substr(indexEnd + 1);

    const auto action = surfaceActions.find(parameter);
    if (action != surfaceActions.end()) {
        action->second(*this, quads[surface], m);
        return;
    }

    const auto handler = surfaceTable.find(parameter);
    if (handler != surfaceTable.end()) {
        handler->second(*this, quads[surface], m);
        if (surface == activeQuad) m_gui.updatePages(quads[activeQuad]);
        return;
    }

    ofLogVerbose("LPMT") << "unhandled OSC address " << address;
}
