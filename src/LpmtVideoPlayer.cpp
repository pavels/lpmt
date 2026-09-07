#include "LpmtVideoPlayerImpl.h"

LpmtVideoPlayer::LpmtVideoPlayer()  : impl(makeLpmtVideoPlayerImpl()) {}
LpmtVideoPlayer::~LpmtVideoPlayer() = default;

void LpmtVideoPlayer::load(const std::string& path) { impl->load(path); }
void LpmtVideoPlayer::play()          { impl->play();  }
void LpmtVideoPlayer::stop()          { impl->stop();  }
void LpmtVideoPlayer::close()         { impl->close(); }
void LpmtVideoPlayer::closeMovie()    { impl->close(); }
void LpmtVideoPlayer::update()        { impl->update(); }

bool LpmtVideoPlayer::isLoaded() const { return impl->isLoaded(); }
bool LpmtVideoPlayer::isPaused() const { return impl->isPaused(); }
void LpmtVideoPlayer::setPaused(bool paused) { impl->setPaused(paused); }

void LpmtVideoPlayer::setSpeed(float speed)       { impl->setSpeed(speed); }
void LpmtVideoPlayer::setVolume(float volume)     { impl->setVolume(volume); }
void LpmtVideoPlayer::setLoopState(ofLoopType s)  { impl->setLoopState(s); }
void LpmtVideoPlayer::setPosition(float pct)      { impl->setPosition(pct); }

float LpmtVideoPlayer::getWidth()  const { return impl->getWidth();  }
float LpmtVideoPlayer::getHeight() const { return impl->getHeight(); }

ofTexture& LpmtVideoPlayer::getTexture() { return impl->getTexture(); }

void LpmtVideoPlayer::draw(float x, float y, float w, float h) {
    impl->draw(x, y, w, h);
}
