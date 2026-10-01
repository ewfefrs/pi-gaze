// Pi-Gaze — annotated debug picture (for the web page) and a BMP encoder.
#pragma once
#include <cstdint>
#include <vector>

#include "pigaze/tracker.hpp"

namespace pigaze {

struct RgbImage {
    int width = 0, height = 0;
    std::vector<uint8_t> px;     // R,G,B per pixel, top row first
    RgbImage() = default;
    RgbImage(int w, int h) : width(w), height(h), px(static_cast<size_t>(w) * h * 3, 24) {}
    void set(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        uint8_t* p = &px[(static_cast<size_t>(y) * width + x) * 3];
        p[0] = r; p[1] = g; p[2] = b;
    }
};

// Top: the frame shrunk by `shrink` with eye windows, pupils and glints marked.
// Bottom: each eye magnified `zoom` times (64x64 px around the pupil).
RgbImage renderDebug(const ImageView& frame, const FrameEyes& eyes, int shrink, int zoom);
std::vector<uint8_t> encodeBmp(const RgbImage& img);

}  // namespace pigaze
