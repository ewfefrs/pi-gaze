// Pi-Gaze — basic geometry and 8-bit grayscale images (no third-party libraries).
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pigaze {

constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    double x = 0, y = 0;
    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double k) const { return {x * k, y * k}; }
    Vec2 operator/(double k) const { return {x / k, y / k}; }
    double norm() const { return std::sqrt(x * x + y * y); }
};
inline double dist(const Vec2& a, const Vec2& b) { return (a - b).norm(); }

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect clipped(int W, int H) const;
    static Rect centered(Vec2 c, int size);   // square of side `size` around c
};

// Non-owning view of 8-bit gray pixels (e.g. the Y plane of a camera frame).
struct ImageView {
    const uint8_t* data = nullptr;
    int width = 0, height = 0, stride = 0;
    bool empty() const { return data == nullptr || width <= 0 || height <= 0; }
    uint8_t at(int x, int y) const { return data[static_cast<size_t>(y) * stride + x]; }
};

struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> px;
    Image() = default;
    Image(int w, int h, uint8_t fill = 0) : width(w), height(h), px(static_cast<size_t>(w) * h, fill) {}
    uint8_t& at(int x, int y) { return px[static_cast<size_t>(y) * width + x]; }
    uint8_t at(int x, int y) const { return px[static_cast<size_t>(y) * width + x]; }
    ImageView view() const { return {px.data(), width, height, width}; }
};

Image crop(const ImageView& src, const Rect& r);          // r is clipped to src
void boxBlur(const Image& src, Image& dst, int radius);    // separable, edges clamped
double bilinear(const Image& img, double x, double y);     // clamped sampling
uint8_t medianOf(const Image& img);

bool writePgm(const std::string& path, const ImageView& img);
bool readPgm(const std::string& path, Image& out);

}  // namespace pigaze
