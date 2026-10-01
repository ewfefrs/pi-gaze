#include "pigaze/calibration.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pigaze {
namespace {

Vec2 wiggle(double tau) {                               // 2 turns per second, 1.2 % of the screen
    const double a = 2 * kPi * 2.0 * tau;
    return {0.012 * std::cos(a), 0.012 * std::sin(a)};
}

double median(std::vector<double> v) {
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

}  // namespace

std::vector<Vec2> CalibrationSession::points(double m) {
    const double a = m, b = 0.5, c = 1 - m;             // snake order: short cursor jumps
    return {{a, a}, {b, a}, {c, a}, {c, b}, {b, b}, {a, b}, {a, c}, {b, c}, {c, c}};
}

CalibrationSession::CalibrationSession(const CalibParams& p, double tStart)
    : p_(p), t0_(tStart), pts_(points(p.margin)) {
    for (auto& s : samples_) s.resize(pts_.size());
}

Vec2 CalibrationSession::update(double t, const GazeFeature f[2]) {
    if (done_) return pts_.back();
    const double rel = t - (t0_ + p_.readyS);
    if (rel < 0) return pts_[0] + wiggle(t - t0_);
    const double per = p_.settleS + p_.collectS;
    const int i = static_cast<int>(rel / per);
    if (i >= count()) { done_ = true; return pts_.back(); }
    idx_ = i;
    const double tau = rel - i * per;
    if (tau >= p_.settleS)
        for (int k = 0; k < 2; ++k)
            if (f[k].valid) samples_[k][i].push_back(f[k].v);
    return tau < p_.wiggleS ? pts_[i] + wiggle(tau) : pts_[i];
}

bool CalibrationSession::result(GazeModel& out, std::string& report) const {
    GazeModel m;
    report.clear();
    for (int k = 0; k < 2; ++k) {
        std::vector<Vec2> in, target;
        for (size_t i = 0; i < pts_.size(); ++i) {
            const auto& s = samples_[k][i];
            if (static_cast<int>(s.size()) < p_.minSamples) continue;
            std::vector<double> xs, ys;
            for (const Vec2& v : s) { xs.push_back(v.x); ys.push_back(v.y); }
            in.push_back({median(xs), median(ys)});
            target.push_back(pts_[i]);
        }
        char buf[128];
        PolyMap map;
        if (in.size() >= 5 && map.fit(in, target)) {
            double se = 0;
            for (size_t i = 0; i < in.size(); ++i) {
                const Vec2 d = map.apply(in[i]) - target[i];
                se += d.x * d.x + d.y * d.y;
            }
            const double rms = std::sqrt(se / static_cast<double>(in.size()));
            const bool good = rms <= p_.maxRms;
            if (good) { m.eye[k] = map; m.rms[k] = rms; }
            std::snprintf(buf, sizeof buf, "eye%d: %zu/%zu points, rms %.1f%%%s", k, in.size(), pts_.size(),
                          rms * 100, good ? "" : " (rejected)");
        } else {
            std::snprintf(buf, sizeof buf, "eye%d: %zu/%zu points (not enough)", k, in.size(), pts_.size());
        }
        report += (k ? "; " : "") + std::string(buf);
    }
    if (!m.ok()) return false;
    out = m;
    return true;
}

}  // namespace pigaze
