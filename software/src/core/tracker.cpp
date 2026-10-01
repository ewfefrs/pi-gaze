#include "pigaze/tracker.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace pigaze {

std::vector<EyeMeasurement> findEyeCandidates(const ImageView& f, const TrackerParams& p) {
    std::vector<EyeMeasurement> found;
    if (f.empty()) return found;
    const int W = f.width, H = f.height;

    // global levels from a sparse histogram
    std::array<size_t, 256> hist{};
    size_t n = 0;
    for (int y = 0; y < H; y += 2)
        for (int x = 0; x < W; x += 2) { hist[f.at(x, y)]++; ++n; }
    auto pct = [&](double q) {
        const size_t k = static_cast<size_t>(q * static_cast<double>(n));
        size_t acc = 0;
        for (int v = 0; v < 256; ++v) { acc += hist[v]; if (acc > k) return v; }
        return 255;
    };
    int thr = std::min(254, std::max(p.det.glintMin, pct(0.5) + p.det.glintDelta));
    size_t above = 0;
    for (int v = thr; v < 256; ++v) above += hist[v];
    if (above > n / 200) thr = std::max(thr, std::min(254, pct(0.999)));   // bright scene

    // small bright blobs with a dark surround = glint candidates
    struct Cand { Vec2 c; int peak; };
    std::vector<Cand> cands;
    std::vector<uint8_t> seen(static_cast<size_t>(W) * H, 0);
    std::vector<int> stack;
    for (int y = 0; y < H; ++y) {
        const uint8_t* row = f.data + static_cast<size_t>(y) * f.stride;
        for (int x = 0; x < W; ++x) {
            if (row[x] < thr || seen[static_cast<size_t>(y) * W + x]) continue;
            int area = 0, peak = 0;
            double sw = 0, sx = 0, sy = 0;
            stack.assign(1, y * W + x);
            seen[static_cast<size_t>(y) * W + x] = 1;
            while (!stack.empty()) {
                const int j = stack.back();
                stack.pop_back();
                const int jx = j % W, jy = j / W, v = f.at(jx, jy);
                ++area;
                peak = std::max(peak, v);
                const double w = v - thr + 1.0;
                sw += w; sx += w * jx; sy += w * jy;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = jx + dx, ny = jy + dy;
                        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                        const size_t k = static_cast<size_t>(ny) * W + nx;
                        if (!seen[k] && f.at(nx, ny) >= thr) { seen[k] = 1; stack.push_back(ny * W + nx); }
                    }
            }
            if (area > p.det.glintMaxArea) continue;
            const Vec2 c(sx / sw, sy / sw);
            double ring = 0;
            int rn = 0;
            for (int k = 0; k < 16; ++k) {
                const double a = 2 * kPi * k / 16;
                const int rx = static_cast<int>(std::lround(c.x + 7.5 * std::cos(a)));
                const int ry = static_cast<int>(std::lround(c.y + 7.5 * std::sin(a)));
                if (rx >= 0 && ry >= 0 && rx < W && ry < H) { ring += f.at(rx, ry); ++rn; }
            }
            if (rn < 8 || ring / rn > peak - p.det.glintDelta) continue;
            cands.push_back({c, peak});
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.peak > b.peak; });
    if (static_cast<int>(cands.size()) > p.maxCandidates) cands.resize(p.maxCandidates);

    for (const Cand& c : cands) {
        EyeMeasurement m = detectEye(f, c.c, p.det);
        if (m.valid && m.hasGlint) found.push_back(m);
    }
    // several glints of one eye lead to the same pupil: keep the most confident one
    std::sort(found.begin(), found.end(),
              [](const EyeMeasurement& a, const EyeMeasurement& b) { return a.confidence > b.confidence; });
    std::vector<EyeMeasurement> uniq;
    for (const EyeMeasurement& m : found) {
        bool dup = false;
        for (const EyeMeasurement& u : uniq)
            if (dist(u.pupil, m.pupil) < std::max(2.0 * std::max(u.radius, m.radius), 6.0)) { dup = true; break; }
        if (!dup) uniq.push_back(m);
    }
    return uniq;
}

EyePair pickEyePair(const std::vector<EyeMeasurement>& c, int frameWidth, const TrackerParams& p) {
    EyePair best;
    double bestScore = -1e9;
    const double dmin = p.ipdMinFrac * frameWidth, dmax = p.ipdMaxFrac * frameWidth;
    for (size_t i = 0; i < c.size(); ++i)
        for (size_t j = i + 1; j < c.size(); ++j) {
            const EyeMeasurement& a = c[i];
            const EyeMeasurement& b = c[j];
            const double dx = std::fabs(a.pupil.x - b.pupil.x), dy = std::fabs(a.pupil.y - b.pupil.y);
            if (dx < dmin || dx > dmax || dy > 0.35 * dx) continue;
            const double rr = std::max(a.radius, b.radius) / std::max(1e-6, std::min(a.radius, b.radius));
            if (rr > 1.8) continue;
            const double score = a.confidence + b.confidence - 0.2 * (rr - 1);
            if (score > bestScore) {
                bestScore = score;
                best.found = true;
                best.eye[0] = a.pupil.x < b.pupil.x ? a : b;
                best.eye[1] = a.pupil.x < b.pupil.x ? b : a;
            }
        }
    return best;
}

void EyeTracker::reset() {
    slot_[0] = slot_[1] = Slot{};
    haveOffset_ = false;
    ipd_ = 0;
    frame_ = 0;
}

FrameEyes EyeTracker::process(const ImageView& f) {
    FrameEyes out;
    const bool due = (frame_++ % p_.searchEvery) == 0;
    const double gmax = ipd_ > 0 ? 0.11 * ipd_ : 0;    // glints lie within ~7 mm of the pupil
    const double minSep = p_.ipdMinFrac * f.width;

    // 1. follow the active eyes in their windows; no glint = not our eye
    for (int k = 0; k < 2; ++k) {
        Slot& s = slot_[k];
        if (!s.active) continue;
        EyeMeasurement m = detectEye(f, s.pos, p_.det, gmax);
        if (!m.hasGlint) m.valid = false;
        out.eye[k] = m;
        if (m.valid) { s.pos = m.pupil; s.lost = 0; }
        else if (++s.lost > p_.lostFrames) s.active = false;
        out.tracked[k] = s.active;
    }
    // 2. both windows slid onto the same eye -> drop the weaker one
    if (slot_[0].active && slot_[1].active && slot_[1].pos.x - slot_[0].pos.x < minSep) {
        const int weak = out.eye[0].confidence < out.eye[1].confidence ? 0 : 1;
        slot_[weak] = Slot{};
        out.eye[weak] = EyeMeasurement{};
        out.tracked[weak] = false;
    }
    // 3. both eyes seen -> inter-eye offset and a smoothed IPD (head-distance scale)
    if (out.eye[0].valid && out.eye[1].valid) {
        offset_ = out.eye[1].pupil - out.eye[0].pupil;
        haveOffset_ = true;
        const double d = offset_.norm();
        ipd_ = ipd_ > 0 ? 0.9 * ipd_ + 0.1 * d : d;
    }
    // 4. one eye lost: look where the other eye + the last offset says it should be
    if (due && haveOffset_ && slot_[0].active != slot_[1].active) {
        const int k = slot_[0].active ? 1 : 0, o = 1 - k;
        const Vec2 expect = k == 1 ? slot_[0].pos + offset_ : slot_[1].pos - offset_;
        EyeMeasurement m = detectEye(f, expect, p_.det, gmax);
        if (m.valid && m.hasGlint && std::fabs(m.pupil.x - slot_[o].pos.x) >= minSep) {
            slot_[k] = Slot{true, m.pupil, 0};
            out.eye[k] = m;
            out.tracked[k] = true;
        }
    }
    // 5. nothing tracked: full-frame search for a pair of eyes
    if (due && !slot_[0].active && !slot_[1].active) {
        out.searched = true;
        const EyePair pr = pickEyePair(findEyeCandidates(f, p_), f.width, p_);
        if (pr.found) {
            for (int k = 0; k < 2; ++k) {
                slot_[k] = Slot{true, pr.eye[k].pupil, 0};
                out.eye[k] = pr.eye[k];
                out.tracked[k] = true;
            }
            offset_ = pr.eye[1].pupil - pr.eye[0].pupil;
            haveOffset_ = true;
            ipd_ = offset_.norm();                        // new acquisition: fresh scale
        }
    }
    out.ipdPx = ipd_;
    return out;
}

}  // namespace pigaze
