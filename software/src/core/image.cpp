#include "pigaze/image.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>

namespace pigaze {

Rect Rect::clipped(int W, int H) const {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(W, x + w), y1 = std::min(H, y + h);
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

Rect Rect::centered(Vec2 c, int size) {
    int x0 = static_cast<int>(std::lround(c.x)) - size / 2;
    int y0 = static_cast<int>(std::lround(c.y)) - size / 2;
    return {x0, y0, size, size};
}

Image crop(const ImageView& src, const Rect& r0) {
    Rect r = r0.clipped(src.width, src.height);
    Image out(r.w, r.h);
    for (int y = 0; y < r.h; ++y) {
        const uint8_t* s = src.data + static_cast<size_t>(r.y + y) * src.stride + r.x;
        std::copy(s, s + r.w, out.px.begin() + static_cast<size_t>(y) * r.w);
    }
    return out;
}

void boxBlur(const Image& src, Image& dst, int radius) {
    const int W = src.width, H = src.height;
    if (radius <= 0 || W == 0 || H == 0) { dst = src; return; }
    std::vector<int> tmp(static_cast<size_t>(W) * H);
    const int n = 2 * radius + 1;
    for (int y = 0; y < H; ++y) {                 // horizontal pass (sums)
        const uint8_t* row = &src.px[static_cast<size_t>(y) * W];
        int s = 0;
        for (int k = -radius; k <= radius; ++k) s += row[std::clamp(k, 0, W - 1)];
        for (int x = 0; x < W; ++x) {
            tmp[static_cast<size_t>(y) * W + x] = s;
            s += row[std::min(x + radius + 1, W - 1)] - row[std::max(x - radius, 0)];
        }
    }
    dst = Image(W, H);
    for (int x = 0; x < W; ++x) {                 // vertical pass
        int s = 0;
        for (int k = -radius; k <= radius; ++k) s += tmp[static_cast<size_t>(std::clamp(k, 0, H - 1)) * W + x];
        for (int y = 0; y < H; ++y) {
            dst.px[static_cast<size_t>(y) * W + x] = static_cast<uint8_t>((s + n * n / 2) / (n * n));
            s += tmp[static_cast<size_t>(std::min(y + radius + 1, H - 1)) * W + x]
               - tmp[static_cast<size_t>(std::max(y - radius, 0)) * W + x];
        }
    }
}

double bilinear(const Image& img, double x, double y) {
    x = std::clamp(x, 0.0, img.width - 1.0);
    y = std::clamp(y, 0.0, img.height - 1.0);
    int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    int x1 = std::min(x0 + 1, img.width - 1), y1 = std::min(y0 + 1, img.height - 1);
    double fx = x - x0, fy = y - y0;
    double a = img.at(x0, y0) * (1 - fx) + img.at(x1, y0) * fx;
    double b = img.at(x0, y1) * (1 - fx) + img.at(x1, y1) * fx;
    return a * (1 - fy) + b * fy;
}

uint8_t medianOf(const Image& img) {
    std::array<size_t, 256> hist{};
    for (uint8_t v : img.px) hist[v]++;
    size_t half = img.px.size() / 2, acc = 0;
    for (int v = 0; v < 256; ++v) {
        acc += hist[v];
        if (acc > half) return static_cast<uint8_t>(v);
    }
    return 255;
}

bool writePgm(const std::string& path, const ImageView& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P5\n" << img.width << " " << img.height << "\n255\n";
    for (int y = 0; y < img.height; ++y)
        f.write(reinterpret_cast<const char*>(img.data + static_cast<size_t>(y) * img.stride), img.width);
    return static_cast<bool>(f);
}

bool readPgm(const std::string& path, Image& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string magic;
    int w = 0, h = 0, maxv = 0;
    f >> magic;
    auto skip = [&f]() {                           // whitespace and # comments
        while (true) {
            int c = f.peek();
            if (c == '#') { std::string line; std::getline(f, line); }
            else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') f.get();
            else break;
        }
    };
    skip(); f >> w; skip(); f >> h; skip(); f >> maxv;
    f.get();
    if (magic != "P5" || w <= 0 || h <= 0 || maxv != 255) return false;
    out = Image(w, h);
    f.read(reinterpret_cast<char*>(out.px.data()), static_cast<std::streamsize>(out.px.size()));
    return static_cast<bool>(f);
}

}  // namespace pigaze
