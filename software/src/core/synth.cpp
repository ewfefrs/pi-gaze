#include "pigaze/synth.hpp"

#include <algorithm>
#include <cmath>

namespace pigaze {
namespace {

// Deterministic Gaussian noise (xorshift + Box-Muller) so tests are reproducible.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
    double uni() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s + 0.5) / 4294967296.0;
    }
    double gauss() { return std::sqrt(-2 * std::log(uni())) * std::cos(2 * kPi * uni()); }
};

// Level of one point: skin, then eye layers (sclera, iris, pupil) inside the opening.
double levelAt(const SynthScene& s, double x, double y) {
    double v = s.skinLevel;
    for (const auto& e : s.eyes) {
        const double dx = x - e.pupil.x, dy = y - e.pupil.y;
        if ((dx * dx) / (e.eyeW * e.eyeW) + (dy * dy) / (e.eyeH * e.eyeH) > 1) continue;
        if (dy < -e.lidTop) continue;                    // covered by the upper lid
        const double r = std::sqrt(dx * dx + dy * dy);
        v = r <= e.pupilR ? e.pupilLevel : r <= e.irisR ? e.irisLevel : e.scleraLevel;
    }
    for (const auto& sp : s.spots)
        if (std::hypot(x - sp.c.x, y - sp.c.y) <= sp.r) v = sp.level;
    return v;
}

}  // namespace

Image renderScene(const SynthScene& s) {
    Image img(s.width, s.height);
    Rng rng(s.seed);
    const int S = 4;                                     // 4x4 supersampling = anti-aliased edges
    for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x) {
            double acc = 0;
            for (int j = 0; j < S; ++j)
                for (int i = 0; i < S; ++i)
                    acc += levelAt(s, x + (i + 0.5) / S - 0.5, y + (j + 0.5) / S - 0.5);
            double v = acc / (S * S);
            for (const auto& e : s.eyes)
                for (const Vec2& g : e.glints) {
                    const double d2 = (x - g.x) * (x - g.x) + (y - g.y) * (y - g.y);
                    v += 400.0 * std::exp(-d2 / (2 * e.glintSigma * e.glintSigma));
                }
            v += s.noiseSigma * rng.gauss();
            img.at(x, y) = static_cast<uint8_t>(std::clamp(std::lround(v), 0L, 255L));
        }
    return img;
}

}  // namespace pigaze
