#ifndef WITH_HWDECODE

#include "LpmtVideoPlayerImpl.h"

namespace {

struct OfPlayerImpl : LpmtVideoPlayer::Impl {
    ofVideoPlayer p;

    void load(const std::string& path) override { p.load(path); }
    void play()  override { p.play(); }
    void stop()  override { p.stop(); }
    void close() override { p.close(); }
    void update() override { p.update(); }

    bool isLoaded() const override { return const_cast<ofVideoPlayer&>(p).isLoaded(); }
    bool isPaused() const override { return const_cast<ofVideoPlayer&>(p).isPaused(); }
    void setPaused(bool paused)     override { p.setPaused(paused); }

    void setSpeed(float speed)      override { p.setSpeed(speed); }
    void setVolume(float volume)    override { p.setVolume(volume); }
    void setLoopState(ofLoopType s) override { p.setLoopState(s); }
    void setPosition(float pct)     override { p.setPosition(pct); }

    float getWidth()  const override { return const_cast<ofVideoPlayer&>(p).getWidth();  }
    float getHeight() const override { return const_cast<ofVideoPlayer&>(p).getHeight(); }

    ofTexture& getTexture() override { return p.getTexture(); }

    void draw(float x, float y, float w, float h) override { p.draw(x, y, w, h); }
};

}

std::shared_ptr<LpmtVideoPlayer::Impl> makeLpmtVideoPlayerImpl() {
    return std::make_shared<OfPlayerImpl>();
}

#endif
