#include "pigaze/ch9329.hpp"

#include <algorithm>
#include <cmath>

namespace pigaze {
namespace ch9329 {

std::vector<uint8_t> frame(uint8_t cmd, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> f = {0x57, 0xAB, 0x00, cmd, static_cast<uint8_t>(data.size())};
    f.insert(f.end(), data.begin(), data.end());
    unsigned sum = 0;
    for (uint8_t b : f) sum += b;
    f.push_back(static_cast<uint8_t>(sum & 0xFF));
    return f;
}

std::vector<uint8_t> mouseAbs(uint8_t buttons, uint16_t x, uint16_t y, int8_t wheel) {
    return frame(kMouseAbs, {0x02, buttons, static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>(x >> 8),
                             static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>(y >> 8),
                             static_cast<uint8_t>(wheel)});
}

std::vector<uint8_t> getInfo() { return frame(kGetInfo, {}); }

uint16_t toAbs(double n) {
    return static_cast<uint16_t>(std::lround(std::clamp(n, 0.0, 1.0) * kAbsMax));
}

bool parseInfo(const std::vector<uint8_t>& rx, Info& out) {
    // reply: 57 AB 00 81 08 <version> <usb status> <leds> <5 reserved> <sum>
    for (size_t i = 0; i + 14 <= rx.size(); ++i) {
        if (rx[i] != 0x57 || rx[i + 1] != 0xAB || rx[i + 3] != 0x81 || rx[i + 4] != 0x08) continue;
        unsigned sum = 0;
        for (size_t k = i; k < i + 13; ++k) sum += rx[k];
        if ((sum & 0xFF) != rx[i + 13]) continue;
        out.version = rx[i + 5];
        out.usbConnected = rx[i + 6] == 0x01;
        return true;
    }
    return false;
}

}  // namespace ch9329

void HidMouse::drain() {
    uint8_t buf[256];
    while (link_.read(buf, sizeof buf) > 0) {}          // command ACKs from the chip: discard
}

void HidMouse::moveTo(const Vec2& n) {
    drain();
    const int x = ch9329::toAbs(n.x), y = ch9329::toAbs(n.y);
    if (x == x_ && y == y_) return;
    if (link_.queued() > 26) { ++dropped_; return; }   // > 2 frames still on the wire
    const auto f = ch9329::mouseAbs(0, static_cast<uint16_t>(x), static_cast<uint16_t>(y));
    if (!link_.write(f.data(), f.size())) { ++dropped_; return; }
    x_ = x; y_ = y;
    ++sent_;
}

void HidMouse::click() {
    const uint16_t x = static_cast<uint16_t>(x_ < 0 ? 2048 : x_), y = static_cast<uint16_t>(y_ < 0 ? 2048 : y_);
    const auto down = ch9329::mouseAbs(0x01, x, y), up = ch9329::mouseAbs(0x00, x, y);
    link_.write(down.data(), down.size());
    link_.write(up.data(), up.size());
    sent_ += 2;
}

}  // namespace pigaze
