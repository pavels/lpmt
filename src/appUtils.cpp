#include "ofApp.h"

//-----------------------------------------------------------
void ofApp::openImageFile()
{
    ofLogNotice() << "Open Image file dialogue";
    ofFileDialogResult dialog_result = ofSystemLoadDialog("Load image file");

    if (dialog_result.bSuccess) {
        quads[activeQuad].loadImageFromFile(dialog_result.getName(), dialog_result.getPath());
        ofLogNotice() << "Loaded image: \"" << dialog_result.getPath() << "\"";
    }
}

//-----------------------------------------------------------
void ofApp::openVideoFile()
{
    ofFileDialogResult dialog_result = ofSystemLoadDialog("Load video file");

    if (dialog_result.bSuccess) {
        quads[activeQuad].loadVideoFromFile(dialog_result.getName(), dialog_result.getPath());
        ofLogNotice() << "Loaded video: \"" << dialog_result.getPath() << "\"";
    }
}

//-----------------------------------------------------------
void ofApp::loadSlideshow()
{
    ofFileDialogResult dialog_result = ofSystemLoadDialog("Find slideshow folder", true, "data"); // TODO: test if the default path works on linux, it doesn't seem to on windows

    if (dialog_result.bSuccess) {
        const std::string slideshowFolderName = dialog_result.getPath();
        quads[activeQuad].slideshowName = slideshowFolderName;
        ofLogNotice() << "Set slide show folder: \"" << slideshowFolderName << "\"";
    }
}

//-----------------------------------------------------------
void ofApp::openSharedVideoFile(int i)
{
    ofFileDialogResult dialog_result = ofSystemLoadDialog("Load shared video file");
    if (dialog_result.bSuccess) {
        if (sharedVideos[i].isLoaded()) {
            sharedVideos[i].closeMovie();
        }
        std::string path = dialog_result.getPath();
        sharedVideos[i].load(path);
        if (sharedVideos[i].isLoaded()) {
            ofLogNotice() << "Loaded shared video #" << i + 1 << ": \"" << path << "\"";
            sharedVideosFiles[i] = path;
            sharedVideos[i].setLoopState(OF_LOOP_NORMAL);
            sharedVideos[i].play();
            // sharedVideos[i].setVolume(0);
        }
    }
}

//-----------------------------------------------------------
void ofApp::openSharedVideoFile(std::string path, int i)
{
    if (sharedVideos[i].isLoaded()) {
        sharedVideos[i].closeMovie();
    }
    sharedVideos[i].load(path);
    if (sharedVideos[i].isLoaded()) {
        ofLogNotice() << "Loaded shared video: #" << i + 1 << ": \"" << path << "\"";
        sharedVideosFiles[i] = path;
        sharedVideos[i].setLoopState(OF_LOOP_NORMAL);
        sharedVideos[i].play();
        //sharedVideos[i].setVolume(0);
    }
}

//-----------------------------------------------------------
ofImage ofApp::loadImageFromFile()
{
    ofImage img;
    ofFileDialogResult dialog_result = ofSystemLoadDialog("Load image file", false);
    if (dialog_result.bSuccess) {
        string imgName = dialog_result.getName();
        string imgPath = dialog_result.getPath();
        img.load(imgPath);
        return img;
    }
    return img;
}

//--------------------------------------------------------------
void ofApp::resync()
{
    if (useTimeline) {
        timeline.setCurrentTimeSeconds(0.0);
    }

    for (int i = 0; i < MAX_QUADS; i++) {
        if (quads[i].initialized) {
            // resets video to start ing point
            if (quads[i].videoBg && quads[i].video.isLoaded()) {
                quads[i].video.setPosition(0.0);
            }
            // resets slideshow to first slide
            if (quads[i].slideshowBg) {
                quads[i].currentSlideId = 0;
                quads[i].slideTimer = 0;
            }
        }
    }
    for (int i = 0; i < MAX_SHARED_VIDEOS; i++) {
        if (sharedVideos[i].isLoaded()) {
            sharedVideos[i].setPosition(0.0);
        }
    }
}

//--------------------------------------------------------------
void ofApp::startProjection()
{
    bStarted = true;
    if (useTimeline) {
        timeline.enable();
        timeline.play();
    }
    for (int i = 0; i < MAX_QUADS; i++) {
        if (quads[i].initialized) {
            quads[i].isOn = true;
            if (quads[i].videoBg && quads[i].video.isLoaded()) {
                quads[i].video.setVolume(quads[i].videoVolume);
                quads[i].video.play();
            }
        }
    }
    for (int i = 0; i < MAX_SHARED_VIDEOS; i++) {
        if (sharedVideos[i].isLoaded()) {
            sharedVideos[i].play();
        }
    }
}

//--------------------------------------------------------------
void ofApp::stopProjection()
{
    bStarted = false;
    if (useTimeline) {
        timeline.stop();
        timeline.hide();
        timeline.disable();
    }
    for (int i = 0; i < MAX_QUADS; i++) {
        if (quads[i].initialized) {
            quads[i].isOn = false;
            if (quads[i].videoBg && quads[i].video.isLoaded()) {
                quads[i].video.setVolume(0);
                quads[i].video.stop();
            }
        }
    }
    for (int i = 0; i < MAX_SHARED_VIDEOS; i++) {
        if (sharedVideos[i].isLoaded()) {
            sharedVideos[i].stop();
        }
    }
}

//--------------------------------------------------------------
void ofApp::copyQuadSettings(int sourceQuad)
{
    if ((sourceQuad >= 0) && (sourceQuad < MAX_QUADS) && quads[sourceQuad].initialized && hasActiveQuad()) {
        int layer = quads[activeQuad].layer;
        int quadNumber = quads[activeQuad].quadNumber;

        ofPoint corners[4];
        for (int i = 0; i < 4; i++) {
            corners[i] = quads[activeQuad].corners[i];
        }

        quads[activeQuad] = quads[sourceQuad];

        quads[activeQuad].layer = layer;
        quads[activeQuad].quadNumber = quadNumber;
        quads[activeQuad].isActive = true;

        for (int i = 0; i < 4; i++) {
            quads[activeQuad].corners[i] = corners[i];
        }
    }
}

//---------------------------------------------------------------
int ofApp::countInitializedQuads() const
{
    int count = 0;
    for (int i = 0; i < MAX_QUADS; i++) {
        if (quads[i].initialized) {
            count++;
        }
    }
    return count;
}

//---------------------------------------------------------------
bool ofApp::hasActiveQuad() const
{
    return (activeQuad >= 0) && (activeQuad < MAX_QUADS) && quads[activeQuad].initialized;
}

//---------------------------------------------------------------
void ofApp::setActiveQuad(int index)
{
    if ((index < 0) || (index >= MAX_QUADS) || !quads[index].initialized) {
        return;
    }

    if ((activeQuad >= 0) && (activeQuad < MAX_QUADS)) {
        quads[activeQuad].isActive = false;
    }
    activeQuad = index;
    quads[activeQuad].isActive = true;
    m_gui.updatePages(quads[activeQuad]);
}

//---------------------------------------------------------------
void ofApp::addQuad()
{
    if (!isEditMode) {
        return;
    }

    int index = -1;
    for (int i = 0; i < MAX_QUADS; i++) {
        if (!quads[i].initialized) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return;
    }

    quads[index].setup(ofPoint(0.25, 0.25), ofPoint(0.75, 0.25), ofPoint(0.75, 0.75), ofPoint(0.25, 0.75), edgeBlendShader, quadMaskShader, surfaceShader, crossfadeShader, m_cameras, ttf);
    quads[index].quadNumber = index;

    quads[index].layer = -1;
    for (int i = 0; i < MAX_QUADS; i++) {
        if (layers[i] == -1) {
            layers[i] = index;
            quads[index].layer = i;
            break;
        }
    }

    setActiveQuad(index);
    nOfQuads = countInitializedQuads();

    if (!timelineHasQuadPage(index)) {
        timelineAddQuadPage(index);
    }

    // next line fixes a bug i've been tracking down for a looong time
    glDisable(GL_DEPTH_TEST);
}

//---------------------------------------------------------------
void ofApp::deleteQuad()
{
    if (!hasActiveQuad()) {
        return;
    }

    const int deletedQuad = activeQuad;

    if (timelineHasQuadPage(deletedQuad)) {
        timelineRemoveQuadPage(deletedQuad);
    }

    if ((quads[deletedQuad].layer >= 0) && (quads[deletedQuad].layer < MAX_QUADS)) {
        layers[quads[deletedQuad].layer] = -1;
    }
    quads[deletedQuad].reset();
    nOfQuads = countInitializedQuads();

    for (int step = 1; step <= MAX_QUADS; step++) {
        const int candidate = (deletedQuad + step) % MAX_QUADS;
        if (quads[candidate].initialized) {
            activeQuad = deletedQuad; // reset() already cleared isActive on it
            setActiveQuad(candidate);
            break;
        }
    }

    // next line fixes a bug i've been tracking down for a looong time
    glDisable(GL_DEPTH_TEST);
}

//---------------------------------------------------------------
void ofApp::setEditMode(bool wanted)
{
    isEditMode = wanted;
    for (int i = 0; i < MAX_QUADS; i++) {
        if (quads[i].initialized) {
            quads[i].isEditMode = wanted;
        }
    }
}

//---------------------------------------------------------------
void ofApp::setMaskSetup(bool wanted)
{
    if (bGui) {
        return;
    }

    setMaskEditMode(wanted);
}

//---------------------------------------------------------------
void ofApp::setFullscreen(bool wanted)
{
    bFullscreen = wanted;
    if (wanted) {
        ofSetFullscreen(true);
    } else {
        ofSetWindowShape(default_window_width, default_window_height);
        ofSetFullscreen(false);
        const int screenW = ofGetScreenWidth();
        const int screenH = ofGetScreenHeight();
        ofSetWindowPosition(screenW / 2 - default_window_width / 2, screenH / 2 - default_window_height / 2);
    }
}

//---------------------------------------------------------------
void ofApp::setTimelineVisible(bool wanted)
{
    if (wanted == bTimeline) {
        return;
    }

    bTimeline = wanted;
    timeline.toggleShow();
    if (bTimeline) {
        timeline.enable();
        m_gui.hide();
        bGui = false;
    } else {
        timeline.disable();
    }
}

//---------------------------------------------------------------
void ofApp::deleteAllQuads()
{
    for (int i = 0; i < MAX_QUADS; i++) {
        if (!quads[i].initialized) {
            continue;
        }

        if (timelineHasQuadPage(i)) {
            timelineRemoveQuadPage(i);
        }
        if ((quads[i].layer >= 0) && (quads[i].layer < MAX_QUADS)) {
            layers[quads[i].layer] = -1;
        }
        quads[i].reset();
    }
    nOfQuads = countInitializedQuads();

    // next line fixes a bug i've been tracking down for a looong time
    glDisable(GL_DEPTH_TEST);
}

//---------------------------------------------------------------
void ofApp::activateNextQuad()
{
    if (!isEditMode) {
        return;
    }

    const int from = ((activeQuad % MAX_QUADS) + MAX_QUADS) % MAX_QUADS;
    for (int step = 1; step <= MAX_QUADS; step++) {
        const int candidate = (from + step) % MAX_QUADS;
        if (quads[candidate].initialized) {
            setActiveQuad(candidate);
            return;
        }
    }
}

//---------------------------------------------------------------
void ofApp::activatePrevQuad()
{
    if (!isEditMode) {
        return;
    }

    const int from = ((activeQuad % MAX_QUADS) + MAX_QUADS) % MAX_QUADS;
    for (int step = 1; step <= MAX_QUADS; step++) {
        const int candidate = (from - step + 2 * MAX_QUADS) % MAX_QUADS;
        if (quads[candidate].initialized) {
            setActiveQuad(candidate);
            return;
        }
    }
}
