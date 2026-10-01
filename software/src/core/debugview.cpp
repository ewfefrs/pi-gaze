#include "pigaze/debugview.hpp"

#include <algorithm>
#include <cmath>

namespace pigaze {
namespace {

struct Color { uint8_t r, g, b; };
const Color kRoi{230, 200, 40}, kPupil{255, 60, 60}, kGlint{60, 255, 90};

void circle(RgbImage& im, double cx, double cy, double r, Color c) {
    const int n = std::max(16, static_cast<int>(2 * kPi * r));
    for (int i = 0; i < n; ++i) {
        const double a = 2 * kPi * i / n;
        im.set(static_cast<int>(std::lround(cx + r * std::cos(a))), static_cast<int>(std::lround(cy + r * std::sin(a))),
               c.r, c.g, c.b);
    }
}

void cross(RgbImage& im, double cx, double cy, int s, Color c) {
    const int x = static_cast<int>(std::lround(cx)), y = static_cast<int>(std::lround(cy));
    for (int d = -s; d <= s; ++d) { im.set(x + d, y, c.r, c.g, c.b); im.set(x, y + d, c.r, c.g, c.b); }
}

void rect(RgbImage& im, int x0, int y0, int x1, int y1, Color c) {
    for (int x = x0; x <= x1; ++x) { im.set(x, y0, c.r, c.g, c.b); im.set(x, y1, c.r, c.g, c.b); }
    for (int y = y0; y <= y1; ++y) { im.set(x0, y, c.r, c.g, c.b); im.set(x1, y, c.r, c.g, c.b); }
}

}  // namespace

RgbImage renderDebug(const ImageView& f, const FrameEyes& e, int shrink, int zoom) {
    shrink = std::max(1, shrink);
    zoom = std::max(1, zoom);
    const int tw = f.width / shrink, th = f.height / shrink, zs = 64 * zoom;
    RgbImage im(std::max(tw, 2 * zs + 8), th + 8 + zs);

    for (int ty = 0; ty < th; ++ty)                      // shrunk frame (block average)
        for (int tx = 0; tx < tw; ++tx) {
            int s = 0;
            for (int y = 0; y < shrink; ++y)
                for (int x = 0; x < shrink; ++x) s += f.at(tx * shrink + x, ty * shrink + y);
            const uint8_t v = static_cast<uint8_t>(s / (shrink * shrink));
            im.set(tx, ty, v, v, v);
        }
    for (int k = 0; k < 2; ++k) {
        const EyeMeasurement& m = e.eye[k];
        if (!e.tracked[k] && !m.valid) continue;
        rect(im, m.roi.x / shrink, m.roi.y / shrink, (m.roi.x + m.roi.w) / shrink, (m.roi.y + m.roi.h) / shrink, kRoi);
        if (m.valid) circle(im, m.pupil.x / shrink, m.pupil.y / shrink, std::max(2.0, m.radius / shrink), kPupil);
        if (m.valid && m.hasGlint) cross(im, m.glint.x / shrink, m.glint.y / shrink, 2, kGlint);
    }

    for (int k = 0; k < 2; ++k) {                        // magnified eyes
        const EyeMeasurement& m = e.eye[k];
        if (!e.tracked[k] && !m.valid) continue;
        const Vec2 c = m.valid ? m.pupil : Vec2(m.roi.x + m.roi.w / 2.0, m.roi.y + m.roi.h / 2.0);
        const int x0 = static_cast<int>(std::floor(c.x)) - 32, y0 = static_cast<int>(std::floor(c.y)) - 32;
        const int px = k * (zs + 8), py = th + 8;
        for (int zy = 0; zy < zs; ++zy)
            for (int zx = 0; zx < zs; ++zx) {
                const int sx = x0 + zx / zoom, sy = y0 + zy / zoom;
                if (sx < 0 || sy < 0 || sx >= f.width || sy >= f.height) continue;
                const uint8_t v = f.at(sx, sy);
                im.set(px + zx, py + zy, v, v, v);
            }
        auto map = [&](const Vec2& u) { return Vec2(px + (u.x - x0 + 0.5) * zoom, py + (u.y - y0 + 0.5) * zoom); };
        if (m.valid) {
            const Vec2 pc = map(m.pupil);
            circle(im, pc.x, pc.y, m.radius * zoom, kPupil);
            cross(im, pc.x, pc.y, 3, kPupil);
            if (m.hasGlint) { const Vec2 g = map(m.glint); cross(im, g.x, g.y, 4, kGlint); }
        }
    }
    return im;
}

std::vector<uint8_t> encodeBmp(const RgbImage& img) {
    const int rowBytes = (img.width * 3 + 3) & ~3;
    const uint32_t dataSize = static_cast<uint32_t>(rowBytes) * img.height, fileSize = 54 + dataSize;
    std::vector<uint8_t> out(fileSize, 0);
    auto put32 = [&out](size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) out[at + i] = static_cast<uint8_t>(v >> (8 * i)); };
    auto put16 = [&out](size_t at, uint16_t v) { out[at] = static_cast<uint8_t>(v); out[at + 1] = static_cast<uint8_t>(v >> 8); };
    out[0] = 'B'; out[1] = 'M';
    put32(2, fileSize);
    put32(10, 54);                                       // pixel data offset
    put32(14, 40);                                       // BITMAPINFOHEADER
    put32(18, static_cast<uint32_t>(img.width));
    put32(22, static_cast<uint32_t>(img.height));        // positive = bottom-up rows
    put16(26, 1);
    put16(28, 24);
    put32(34, dataSize);
    put32(38, 2835); put32(42, 2835);                    // 72 dpi
    for (int y = 0; y < img.height; ++y) {
        uint8_t* dst = &out[54 + static_cast<size_t>(img.height - 1 - y) * rowBytes];
        const uint8_t* src = &img.px[static_cast<size_t>(y) * img.width * 3];
        for (int x = 0; x < img.width; ++x) {
            dst[x * 3] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3];
        }
    }
    return out;
}

}  // namespace pigaze
