#pragma once

#include "ofMain.h"
#include <memory>
#include <string>
#include <vector>

class LpmtNdiSource {
public:
    LpmtNdiSource();
    ~LpmtNdiSource();

    // a copied surface gets its own receiver - sharing one would make both
    // surfaces fight over the sender name
    LpmtNdiSource(const LpmtNdiSource&);
    LpmtNdiSource& operator=(const LpmtNdiSource&);
    LpmtNdiSource(LpmtNdiSource&&) noexcept = default;
    LpmtNdiSource& operator=(LpmtNdiSource&&) noexcept = default;

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
