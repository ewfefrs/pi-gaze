#include "pigaze/gaze.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "pigaze/linalg.hpp"

namespace pigaze {

GazeFeature makeFeature(const EyeMeasurement& m, double ipdPx) {
    GazeFeature f;
    if (!m.valid || !m.hasGlint || ipdPx <= 0) return f;
    f.valid = true;
    f.v = (m.pupil - m.glint) * (100.0 / ipdPx);
    return f;
}

void PolyMap::basis(const Vec2& v, double* t) const {
    const double x = (v.x - mean_.x) / scale_, y = (v.y - mean_.y) / scale_;
    t[0] = 1; t[1] = x; t[2] = y;
    t[3] = x * y; t[4] = x * x; t[5] = y * y;
}

bool PolyMap::fit(const std::vector<Vec2>& in, const std::vector<Vec2>& out, double ridge) {
    ok_ = false;
    const int m = static_cast<int>(in.size());
    if (m < 3 || out.size() != in.size()) return false;
    terms_ = m >= 6 ? 6 : 3;
    // normalise inputs: centre + common scale (keeps the quadratic terms well conditioned)
    mean_ = {};
    for (const Vec2& v : in) mean_ = mean_ + v;
    mean_ = mean_ / m;
    double s = 0;
    for (const Vec2& v : in) s += (v - mean_).norm();
    scale_ = s > 1e-9 ? s / m : 1.0;

    std::vector<double> X(static_cast<size_t>(m) * terms_), yx(m), yy(m), cx, cy;
    double t[kTerms];
    for (int i = 0; i < m; ++i) {
        basis(in[i], t);
        for (int k = 0; k < terms_; ++k) X[static_cast<size_t>(i) * terms_ + k] = t[k];
        yx[i] = out[i].x;
        yy[i] = out[i].y;
    }
    std::vector<bool> unpenalised(terms_, false);
    unpenalised[0] = true;                              // never shrink the offset
    if (!leastSquares(X, yx, m, terms_, cx, ridge, unpenalised) ||
        !leastSquares(X, yy, m, terms_, cy, ridge, unpenalised))
        return false;
    cx_.fill(0); cy_.fill(0);
    for (int k = 0; k < terms_; ++k) { cx_[k] = cx[k]; cy_[k] = cy[k]; }
    ok_ = true;
    return true;
}

Vec2 PolyMap::apply(const Vec2& v) const {
    double t[kTerms];
    basis(v, t);
    Vec2 r;
    for (int k = 0; k < terms_; ++k) { r.x += cx_[k] * t[k]; r.y += cy_[k] * t[k]; }
    return r;
}

std::string PolyMap::serialize() const {
    std::ostringstream o;
    o.precision(12);
    o << (ok_ ? 1 : 0) << ' ' << terms_ << ' ' << mean_.x << ' ' << mean_.y << ' ' << scale_;
    for (double c : cx_) o << ' ' << c;
    for (double c : cy_) o << ' ' << c;
    return o.str();
}

bool PolyMap::deserialize(const std::string& line) {
    std::istringstream i(line);
    int okv = 0;
    i >> okv >> terms_ >> mean_.x >> mean_.y >> scale_;
    for (double& c : cx_) i >> c;
    for (double& c : cy_) i >> c;
    ok_ = static_cast<bool>(i) && okv == 1 && (terms_ == 3 || terms_ == 6) && scale_ > 0;
    return static_cast<bool>(i);
}

bool GazeModel::predict(const GazeFeature f[2], Vec2& out) const {
    Vec2 sum;
    int n = 0;
    for (int k = 0; k < 2; ++k)
        if (f[k].valid && eye[k].ok()) { sum = sum + eye[k].apply(f[k].v); ++n; }
    if (n == 0) return false;
    out = sum / n;
    return true;
}

bool GazeModel::save(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp);
        if (!f) return false;
        f << "pigaze-calibration 1\n";
        for (int k = 0; k < 2; ++k) f << "eye" << k << ' ' << rms[k] << ' ' << eye[k].serialize() << '\n';
        if (!f) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool GazeModel::load(const std::string& path) {
    std::ifstream f(path);
    std::string header;
    if (!std::getline(f, header) || header != "pigaze-calibration 1") return false;
    GazeModel m;
    for (int k = 0; k < 2; ++k) {
        std::string line, tag;
        if (!std::getline(f, line)) return false;
        std::istringstream i(line);
        i >> tag >> m.rms[k];
        std::string rest;
        std::getline(i, rest);
        if (tag != "eye" + std::to_string(k) || !m.eye[k].deserialize(rest)) return false;
    }
    *this = m;
    return true;
}

}  // namespace pigaze
