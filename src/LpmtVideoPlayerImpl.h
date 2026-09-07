#pragma once

#include "LpmtVideoPlayer.h"

struct LpmtVideoPlayer::Impl {
    virtual ~Impl() = default;

    virtual void load(const std::string& path) = 0;
    virtual void play()  = 0;
    virtual void stop()  = 0;
    virtual void close() = 0;
    virtual void update() = 0;

    virtual bool isLoaded() const = 0;
    virtual bool isPaused() const = 0;
    virtual void setPaused(bool paused) = 0;

    virtual void setSpeed(float speed)      = 0;
    virtual void setVolume(float volume)    = 0;
    virtual void setLoopState(ofLoopType s) = 0;
    virtual void setPosition(float pct)     = 0;

    virtual float getWidth()  const = 0;
    virtual float getHeight() const = 0;

    virtual ofTexture& getTexture() = 0;

    virtual void draw(float x, float y, float w, float h) = 0;
};

std::shared_ptr<LpmtVideoPlayer::Impl> makeLpmtVideoPlayerImpl();
