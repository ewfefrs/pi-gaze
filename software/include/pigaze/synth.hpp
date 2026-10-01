// Pi-Gaze — synthetic IR eye images (tests and `pigaze --selftest`).
#pragma once
#include <cstdint>
#include <vector>

#include "pigaze/image.hpp"

namespace pigaze {

struct SynthEye {
    Vec2 pupil;                  // true pupil centre (px)
    double pupilR = 8;           // px
    double irisR = 20;           // px, centred on the pupil
    double eyeW = 34, eyeH = 16; // eye opening (sclera) semi-axes, centred on the pupil
    std::vector<Vec2> glints;    // glint centres (px)
    double glintSigma = 0.8;     // px
    int pupilLevel = 25, irisLevel = 95, scleraLevel = 175;
    double lidTop = 1e9;         // upper eyelid: pixels above pupil.y - lidTop are skin
};

struct SynthScene {
    int width = 640, height = 360;
    int skinLevel = 140;
    double noiseSigma = 3.0;
    uint32_t seed = 1;
    std::vector<SynthEye> eyes;
    struct Spot { Vec2 c; double r; int level; };
    std::vector<Spot> spots;     // distractors: lamps, glasses reflections, dark blobs
};

Image renderScene(const SynthScene& s);

}  // namespace pigaze
