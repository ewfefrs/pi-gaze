// Pi-Gaze — finding both eyes in a full frame and tracking them frame to frame.
#pragma once
#include <vector>

#include "pigaze/eye_detector.hpp"

namespace pigaze {

struct TrackerParams {
    DetectorParams det;
    double ipdMinFrac = 0.03;    // distance between the eyes, as a fraction of frame width
    double ipdMaxFrac = 0.25;
    int maxCandidates = 150;     // glint candidates examined per full-frame search
    int lostFrames = 12;         // an eye survives this many bad frames (blinks)
    int searchEvery = 3;         // full-frame search cadence while not tracking (frames)
};

// Full-frame search: every small bright blob with local contrast is a potential glint;
// a dark pupil next to it makes it an eye candidate. Sorted by confidence, deduplicated.
std::vector<EyeMeasurement> findEyeCandidates(const ImageView& frame, const TrackerParams& p);

struct EyePair {
    bool found = false;
    EyeMeasurement eye[2];       // [0] = image-left, [1] = image-right
};
EyePair pickEyePair(const std::vector<EyeMeasurement>& cands, int frameWidth, const TrackerParams& p);

struct FrameEyes {
    EyeMeasurement eye[2];       // slot 0 = image-left eye, slot 1 = image-right eye
    bool tracked[2] = {false, false};
    double ipdPx = 0;            // smoothed distance between the pupils (0 = unknown yet)
    bool searched = false;       // a full-frame search ran on this frame
};

class EyeTracker {
public:
    explicit EyeTracker(const TrackerParams& p) : p_(p) {}
    FrameEyes process(const ImageView& frame);
    void reset();
    const TrackerParams& params() const { return p_; }

private:
    struct Slot { bool active = false; Vec2 pos; int lost = 0; };
    TrackerParams p_;
    Slot slot_[2];
    Vec2 offset_;                // slot1 - slot0 from the last frame with both eyes
    bool haveOffset_ = false;
    double ipd_ = 0;
    long frame_ = 0;
};

}  // namespace pigaze
