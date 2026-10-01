// Pi-Gaze — cursor smoothing and dwell detection.
#pragma once
#include "pigaze/image.hpp"

namespace pigaze {

// One Euro filter (Casiez et al. 2012): strong smoothing when still, little lag when moving.
class OneEuro {
public:
    OneEuro(double minCutoff = 1.0, double beta = 0.0, double dCutoff = 1.0)
        : minCutoff_(minCutoff), beta_(beta), dCutoff_(dCutoff) {}
    double filter(double x, double t);
    void reset() { init_ = false; }

private:
    static double alpha(double cutoff, double dt);
    double minCutoff_, beta_, dCutoff_;
    bool init_ = false;
    double x_ = 0, dx_ = 0, t_ = 0;
};

class OneEuro2 {
public:
    OneEuro2(double minCutoff, double beta, double dCutoff)
        : x_(minCutoff, beta, dCutoff), y_(minCutoff, beta, dCutoff) {}
    Vec2 filter(const Vec2& p, double t) { return {x_.filter(p.x, t), y_.filter(p.y, t)}; }
    void reset() { x_.reset(); y_.reset(); }

private:
    OneEuro x_, y_;
};

// Fires once when the point stays within `radius` for `holdS`; re-arms after leaving.
class Dwell {
public:
    Dwell(double holdS, double radius) : hold_(holdS), radius_(radius) {}
    bool update(const Vec2& p, double t);
    void reset() { active_ = false; }
    double progress(double t) const;   // 0..1 of the current hold

private:
    double hold_, radius_;
    bool active_ = false, armed_ = true;
    Vec2 anchor_;
    double t0_ = 0;
};

}  // namespace pigaze
