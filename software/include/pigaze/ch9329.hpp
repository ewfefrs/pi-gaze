// Pi-Gaze — CH9329 UART -> USB HID protocol (absolute mouse).
// Frame: 57 AB | addr 00 | cmd | len | data... | sum (low byte of the sum of all previous bytes)
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "pigaze/controller.hpp"

namespace pigaze {

class ByteLink {
public:
    virtual ~ByteLink() = default;
    virtual bool write(const uint8_t* d, size_t n) = 0;
    virtual int read(uint8_t* d, size_t n) = 0;       // non-blocking; bytes read
    virtual size_t queued() const { return 0; }       // bytes not yet on the wire
};

namespace ch9329 {
constexpr uint8_t kGetInfo = 0x01, kMouseAbs = 0x04, kMouseRel = 0x05;
constexpr int kAbsMax = 4095;
std::vector<uint8_t> frame(uint8_t cmd, const std::vector<uint8_t>& data);
std::vector<uint8_t> mouseAbs(uint8_t buttons, uint16_t x, uint16_t y, int8_t wheel = 0);
std::vector<uint8_t> getInfo();
uint16_t toAbs(double n);                             // 0..1 -> 0..4095

struct Info { uint8_t version = 0; bool usbConnected = false; };
bool parseInfo(const std::vector<uint8_t>& rx, Info& out);   // finds a GET_INFO reply in rx
}  // namespace ch9329

// Cursor output through the CH9329. Moves are dropped (never queued) while the UART is
// still busy, so a slow link cannot build up lag; clicks are always sent.
class HidMouse : public CursorOutput {
public:
    explicit HidMouse(ByteLink& link) : link_(link) {}
    void moveTo(const Vec2& n) override;
    void click() override;
    long sent() const { return sent_; }
    long dropped() const { return dropped_; }

private:
    void drain();
    ByteLink& link_;
    int x_ = -1, y_ = -1;
    long sent_ = 0, dropped_ = 0;
};

}  // namespace pigaze
