// Pi-Gaze — the application logic: modes, calibration, cursor output. No I/O of its own.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "pigaze/calibration.hpp"
#include "pigaze/filter.hpp"
#include "pigaze/gaze.hpp"

namespace pigaze {

// Where cursor commands go (CH9329 on the Pi, a recorder in tests). Coordinates 0..1.
class CursorOutput {
public:
    virtual ~CursorOutput() = default;
    virtual void moveTo(const Vec2& n) = 0;
    virtual void click() = 0;
};

enum class Mode { NeedCalibration, Calibrating, Tracking, Paused };
const char* modeName(Mode m);

enum class Command { Toggle, Calibrate, Pause, Resume };

struct ControllerParams {
    CalibParams calib;
    double minCutoff = 0.6;      // One Euro, in screen fractions
    double beta = 2.0;
    double dCutoff = 1.0;
    bool autoCalibrate = true;   // start calibrating when eyes are seen and there is no calibration
    double autoCalibrateAfterS = 2.0;
    bool dwellClick = false;
    double dwellS = 1.2;
    double dwellRadius = 0.025;
    double gapResetS = 0.3;      // restart the filter after a gap in valid gaze (blink, glance away)
    std::string calibFile;       // empty = do not persist
};

struct ControllerStatus {
    Mode mode = Mode::NeedCalibration;
    bool gazeValid = false;
    Vec2 gaze;                   // filtered, 0..1
    int calibPoint = 0, calibPoints = 0;
    std::string lastCalibReport;
    double rms[2] = {0, 0};
    bool calibrated = false;
};

class Controller {
public:
    Controller(const ControllerParams& p, CursorOutput& out);
    bool loadCalibration();                     // from p.calibFile
    void command(Command c, double t);
    void onFrame(double t, const FrameEyes& eyes);
    Mode mode() const { return mode_; }
    const ControllerStatus& status() const { return status_; }
    const GazeModel& model() const { return model_; }
    std::vector<std::string> takeLog();         // human-readable events since the last call

private:
    void startCalibration(double t);
    void finishCalibration(double t);
    void play(const std::vector<Vec2>& path, double t, double stepS);
    void log(const std::string& s) { log_.push_back(s); }

    ControllerParams p_;
    CursorOutput& out_;
    Mode mode_ = Mode::NeedCalibration;
    Mode beforeCalib_ = Mode::NeedCalibration;
    GazeModel model_;
    std::unique_ptr<CalibrationSession> session_;
    OneEuro2 filter_;
    Dwell dwell_;
    double lastValidT_ = -1e9;
    double eyesSinceT_ = -1;
    bool autoTried_ = false;
    struct Anim { std::vector<Vec2> path; double t0 = 0, step = 0; } anim_;
    ControllerStatus status_;
    std::vector<std::string> log_;
};

}  // namespace pigaze
