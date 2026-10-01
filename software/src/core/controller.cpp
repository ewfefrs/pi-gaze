#include "pigaze/controller.hpp"

#include <algorithm>
#include <cmath>

namespace pigaze {

const char* modeName(Mode m) {
    switch (m) {
        case Mode::NeedCalibration: return "need-calibration";
        case Mode::Calibrating: return "calibrating";
        case Mode::Tracking: return "tracking";
        case Mode::Paused: return "paused";
    }
    return "?";
}

Controller::Controller(const ControllerParams& p, CursorOutput& out)
    : p_(p), out_(out), filter_(p.minCutoff, p.beta, p.dCutoff), dwell_(p.dwellS, p.dwellRadius) {}

bool Controller::loadCalibration() {
    GazeModel m;
    if (p_.calibFile.empty() || !m.load(p_.calibFile) || !m.ok()) return false;
    model_ = m;
    mode_ = Mode::Tracking;
    status_.mode = mode_;
    status_.calibrated = true;
    status_.rms[0] = m.rms[0];
    status_.rms[1] = m.rms[1];
    log("calibration loaded from " + p_.calibFile);
    return true;
}

void Controller::command(Command c, double t) {
    switch (c) {
        case Command::Calibrate:
            startCalibration(t);
            break;
        case Command::Toggle:
            if (mode_ == Mode::Tracking) { mode_ = Mode::Paused; log("paused"); }
            else if (mode_ == Mode::Paused) { mode_ = Mode::Tracking; filter_.reset(); log("resumed"); }
            else if (mode_ == Mode::NeedCalibration) startCalibration(t);
            else { session_.reset(); mode_ = beforeCalib_; log("calibration cancelled"); }
            break;
        case Command::Pause:
            if (mode_ == Mode::Tracking) { mode_ = Mode::Paused; log("paused"); }
            break;
        case Command::Resume:
            if (mode_ == Mode::Paused) { mode_ = Mode::Tracking; filter_.reset(); log("resumed"); }
            break;
    }
    status_.mode = mode_;
}

void Controller::startCalibration(double t) {
    if (mode_ != Mode::Calibrating) beforeCalib_ = mode_;
    session_ = std::make_unique<CalibrationSession>(p_.calib, t);
    mode_ = Mode::Calibrating;
    anim_ = Anim{};
    log("calibration started: follow the cursor with your eyes");
}

void Controller::play(const std::vector<Vec2>& path, double t, double stepS) {
    anim_.path = path;
    anim_.t0 = t;
    anim_.step = stepS;
}

void Controller::finishCalibration(double t) {
    GazeModel m;
    std::string report;
    const bool ok = session_->result(m, report);
    session_.reset();
    status_.lastCalibReport = report;
    std::vector<Vec2> path;
    if (ok) {
        model_ = m;
        mode_ = Mode::Tracking;
        filter_.reset();
        dwell_.reset();
        status_.calibrated = true;
        status_.rms[0] = m.rms[0];
        status_.rms[1] = m.rms[1];
        log("calibration OK: " + report);
        if (!p_.calibFile.empty() && !model_.save(p_.calibFile))
            log("WARNING: cannot save the calibration to " + p_.calibFile);
        for (int i = 0; i <= 24; ++i) {                    // success: a small circle mid-screen
            const double a = 2 * kPi * i / 24;
            path.push_back({0.5 + 0.03 * std::cos(a), 0.5 + 0.05 * std::sin(a)});
        }
        play(path, t, 0.025);
    } else {
        mode_ = !model_.ok() ? Mode::NeedCalibration
                             : beforeCalib_ == Mode::Paused ? Mode::Paused : Mode::Tracking;
        log("calibration FAILED, previous one kept: " + report);
        for (int i = 0; i < 4; ++i) { path.push_back({0.44, 0.5}); path.push_back({0.56, 0.5}); }   // shake
        path.push_back({0.5, 0.5});
        play(path, t, 0.08);
    }
    status_.mode = mode_;
}

void Controller::onFrame(double t, const FrameEyes& eyes) {
    const GazeFeature f[2] = {makeFeature(eyes.eye[0], eyes.ipdPx), makeFeature(eyes.eye[1], eyes.ipdPx)};
    const bool anyEye = f[0].valid || f[1].valid;
    status_.mode = mode_;

    if (!anim_.path.empty()) {                           // feedback animation first
        const size_t i = static_cast<size_t>((t - anim_.t0) / anim_.step);
        if (i < anim_.path.size()) { out_.moveTo(anim_.path[i]); return; }
        anim_.path.clear();
    }

    switch (mode_) {
        case Mode::NeedCalibration:
            status_.gazeValid = false;
            if (p_.autoCalibrate && !autoTried_) {
                if (!anyEye) eyesSinceT_ = -1;
                else if (eyesSinceT_ < 0) eyesSinceT_ = t;
                else if (t - eyesSinceT_ >= p_.autoCalibrateAfterS) {
                    autoTried_ = true;                      // only once per start: never loop
                    startCalibration(t);
                }
            }
            break;
        case Mode::Calibrating: {
            status_.gazeValid = false;
            const Vec2 target = session_->update(t, f);
            status_.calibPoint = session_->index() + 1;
            status_.calibPoints = session_->count();
            if (session_->finished()) finishCalibration(t);
            else out_.moveTo(target);
            break;
        }
        case Mode::Tracking:
        case Mode::Paused: {
            Vec2 g;
            if (!model_.predict(f, g)) {
                status_.gazeValid = false;
                dwell_.reset();
                break;
            }
            if (t - lastValidT_ > p_.gapResetS) filter_.reset();
            lastValidT_ = t;
            g.x = std::clamp(g.x, 0.0, 1.0);
            g.y = std::clamp(g.y, 0.0, 1.0);
            const Vec2 s = filter_.filter(g, t);
            status_.gazeValid = true;
            status_.gaze = s;
            if (mode_ == Mode::Paused) break;               // paused: show gaze, leave the mouse alone
            out_.moveTo(s);
            if (p_.dwellClick && dwell_.update(s, t)) { out_.click(); log("dwell click"); }
            break;
        }
    }
    status_.mode = mode_;
}

std::vector<std::string> Controller::takeLog() {
    std::vector<std::string> r;
    r.swap(log_);
    return r;
}

}  // namespace pigaze
