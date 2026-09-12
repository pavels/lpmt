#pragma once

#include "ofMain.h"
#include <memory>
#include <string>
#include <vector>

class LpmtNdiSource {
public:
    LpmtNdiSource();

    // copies share the receiver; see copyQuadSettings

    void setSource(const std::string& name);
    const std::string& source() const;

    void update();
    void close();

    bool isReady() const;
    float getWidth() const;
    float getHeight() const;

    ofTexture& getTexture();
    void draw(float x, float y, float w, float h);

    struct Impl;

private:
    std::shared_ptr<Impl> impl;
};

namespace LpmtNdi {
std::vector<std::string> findSources();
}
