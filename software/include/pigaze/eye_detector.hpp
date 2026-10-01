// Pi-Gaze — dark-pupil + corneal-glint detection inside one eye window.
#pragma once
#include "pigaze/image.hpp"

namespace pigaze {

struct DetectorParams {
    int roiSize = 120;          // px, square eye window (full-frame resolution)
    double pupilMinR = 2.0;     // px
    double pupilMaxR = 22.0;    // px
    int glintMin = 170;         // a glint is at least this bright ...
    int glintDelta = 60;        // ... and this much brighter than the window median
    int glintMaxArea = 80;      // px; bigger bright blobs are glasses / lamp reflections
    double minConfidence = 0.35;
};

struct EyeMeasurement {
    bool valid = false;         // pupil found and confident
    Vec2 pupil;                 // full-frame px, sub-pixel
    double radius = 0;          // px
    double confidence = 0;      // 0..1
    bool hasGlint = false;
    Vec2 glint;                 // full-frame px: weighted centroid of the LED reflections
    int glintCount = 0;
    Rect roi;                   // window that was searched
};

// Look for a dark pupil (and its corneal glints) in a window around `center`.
// glintMaxDist: how far from the pupil centre a glint may lie (px); 0 = derive from the radius.
EyeMeasurement detectEye(const ImageView& frame, Vec2 center, const DetectorParams& p,
                         double glintMaxDist = 0);

}  // namespace pigaze
