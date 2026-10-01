// Pi-Gaze — 9-point calibration driven by the cursor itself (no PC-side app).
#pragma once
#include <string>
#include <vector>

#include "pigaze/gaze.hpp"

namespace pigaze {

struct CalibParams {
    double margin = 0.1;         // points at margin .. 1-margin of the screen
    double readyS = 1.5;         // before the first point (cursor circles there)
    double wiggleS = 0.45;       // small circle on each new point to catch the eye
    double settleS = 0.9;        // jump -> start of sampling (saccade + fixation)
    double collectS = 1.0;       // sampling time per point
    int minSamples = 8;          // per eye and point
    double maxRms = 0.08;        // accept an eye only if its fit residual is below (screen fraction)
};

class CalibrationSession {
public:
    CalibrationSession(const CalibParams& p, double tStart);
    // Feed one frame; returns where the cursor has to be now (normalised 0..1).
    Vec2 update(double t, const GazeFeature f[2]);
    bool finished() const { return done_; }
    int index() const { return idx_; }
    int count() const { return static_cast<int>(pts_.size()); }
    // Fits the per-eye maps. False if no eye produced a good fit.
    bool result(GazeModel& out, std::string& report) const;
    static std::vector<Vec2> points(double margin);

private:
    CalibParams p_;
    double t0_;
    std::vector<Vec2> pts_;
    std::vector<std::vector<Vec2>> samples_[2];
    int idx_ = 0;
    bool done_ = false;
};

}  // namespace pigaze
