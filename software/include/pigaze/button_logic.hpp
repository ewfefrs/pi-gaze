// Pi-Gaze — single / double / long press classification for the power button.
#pragma once

namespace pigaze {

enum class ButtonEvent { None, Single, Double };

// Long presses (> longS) produce nothing: holding the button is logind's "power off".
class PressClassifier {
public:
    explicit PressClassifier(double doubleS = 0.45, double longS = 1.5) : doubleS_(doubleS), longS_(longS) {}
    void press(double t);
    ButtonEvent release(double t);
    ButtonEvent tick(double t);   // call regularly: a lone press becomes Single after doubleS

private:
    double doubleS_, longS_;
    double pressT_ = -1, lastRelease_ = -1;
    bool pending_ = false;        // a short press waiting for a possible second one
    bool secondPress_ = false;    // the current press came within doubleS of the last release
};

}  // namespace pigaze
