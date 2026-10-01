// Pi-Gaze — from eye measurements to a point on the screen.
#pragma once
#include <array>
#include <string>
#include <vector>

#include "pigaze/tracker.hpp"

namespace pigaze {

// Pupil-centre / corneal-reflection vector of one eye, divided by the distance between the
// pupils (head-distance normalisation). Dimensionless, roughly +-5 over a monitor.
struct GazeFeature {
    bool valid = false;
    Vec2 v;
};
GazeFeature makeFeature(const EyeMeasurement& m, double ipdPx);

// Second-order polynomial map R^2 -> R^2 (affine when there are too few points).
class PolyMap {
public:
    static constexpr int kTerms = 6;
    bool fit(const std::vector<Vec2>& in, const std::vector<Vec2>& out, double ridge = 1e-4);
    Vec2 apply(const Vec2& v) const;
    bool ok() const { return ok_; }
    int terms() const { return terms_; }
    std::string serialize() const;
    bool deserialize(const std::string& line);

private:
    void basis(const Vec2& v, double* t) const;
    bool ok_ = false;
    int terms_ = kTerms;
    Vec2 mean_;
    double scale_ = 1;
    std::array<double, kTerms> cx_{}, cy_{};
};

// One map per eye; prediction = mean of the eyes that are valid and calibrated.
struct GazeModel {
    PolyMap eye[2];
    double rms[2] = {0, 0};       // calibration residual, fraction of the screen
    bool ok() const { return eye[0].ok() || eye[1].ok(); }
    bool predict(const GazeFeature f[2], Vec2& out) const;
    bool save(const std::string& path) const;
    bool load(const std::string& path);
};

}  // namespace pigaze
