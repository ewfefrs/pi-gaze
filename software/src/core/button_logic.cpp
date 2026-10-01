#include "pigaze/button_logic.hpp"

namespace pigaze {

void PressClassifier::press(double t) {
    pressT_ = t;
    secondPress_ = pending_ && t - lastRelease_ < doubleS_;
}

ButtonEvent PressClassifier::release(double t) {
    if (pressT_ < 0) return ButtonEvent::None;
    const bool longPress = t - pressT_ > longS_;
    pressT_ = -1;
    if (longPress) { pending_ = secondPress_ = false; return ButtonEvent::None; }
    if (secondPress_) { pending_ = secondPress_ = false; return ButtonEvent::Double; }
    const ButtonEvent earlier = pending_ ? ButtonEvent::Single : ButtonEvent::None;   // a lone press
    pending_ = true;                                      // this one waits for a possible second press
    lastRelease_ = t;
    return earlier;
}

ButtonEvent PressClassifier::tick(double t) {
    if (pending_ && pressT_ < 0 && t - lastRelease_ >= doubleS_) {
        pending_ = false;
        return ButtonEvent::Single;
    }
    return ButtonEvent::None;
}

}  // namespace pigaze
