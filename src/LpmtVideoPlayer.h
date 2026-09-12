#pragma once

#include "ofMain.h"
#include <memory>
#include <string>

class LpmtVideoPlayer {
public:
    LpmtVideoPlayer();

    // copies share the backend, like ofVideoPlayer; see copyQuadSettings

    void load(const std::string& path);
    void play();
    void stop();
    void close();
    void closeMovie();
    void update();

    bool isLoaded()  const;
    bool isPaused()  const;
    void setPaused(bool paused);

    void setSpeed(float speed);
    void setVolume(float volume);
    void setLoopState(ofLoopType state);
    void setPosition(float pct);

    float getWidth()  const;
    float getHeight() const;

    ofTexture& getTexture();

    void draw(float x, float y, float w, float h);

    struct Impl;

private:
    std::shared_ptr<Impl> impl;
};
