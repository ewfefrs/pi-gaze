#include "pigaze/filter.hpp"

#include <algorithm>
#include <cmath>

namespace pigaze {

double OneEuro::alpha(double cutoff, double dt) {
    const double tau = 1.0 / (2 * kPi * cutoff);
    return 1.0 / (1.0 + tau / dt);
}

double OneEuro::filter(double x, double t) {
    if (!init_ || t <= t_) {
        if (!init_) { x_ = x; dx_ = 0; init_ = true; }
        t_ = t;
        return x_;
    }
    const double dt = t - t_;
    const double dx = (x - x_) / dt;
    dx_ += alpha(dCutoff_, dt) * (dx - dx_);
    const double cutoff = minCutoff_ + beta_ * std::fabs(dx_);
    x_ += alpha(cutoff, dt) * (x - x_);
    t_ = t;
    return x_;
}

bool Dwell::update(const Vec2& p, double t) {
    if (!active_ || dist(p, anchor_) > radius_) {
        if (active_ && dist(p, anchor_) > radius_) armed_ = true;   // left the spot: re-arm
        active_ = true;
        anchor_ = p;
        t0_ = t;
        return false;
    }
    if (armed_ && t - t0_ >= hold_) { armed_ = false; return true; }
    return false;
}

double Dwell::progress(double t) const {
    if (!active_ || !armed_) return 0;
    return std::clamp((t - t0_) / hold_, 0.0, 1.0);
}

}  // namespace pigaze
