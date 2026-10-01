#include <algorithm>

#include "pigaze/eye_detector.hpp"
#include "pigaze/synth.hpp"
#include "pigaze/tracker.hpp"
#include "testing.hpp"

using namespace pigaze;

namespace {

SynthEye eyeAt(Vec2 c, double r = 8, Vec2 glintOff = {3.0, 4.0}) {
    SynthEye e;
    e.pupil = c;
    e.pupilR = r;
    e.irisR = std::max(2.4 * r, 16.0);
    e.eyeW = e.irisR * 1.9;
    e.eyeH = e.irisR * 0.95;
    e.glints = {c + glintOff + Vec2(-1.3, 0), c + glintOff + Vec2(1.3, 0)};   // two LEDs
    return e;
}

SynthScene oneEye(const SynthEye& e, int w = 200, int h = 160, uint32_t seed = 7) {
    SynthScene s;
    s.width = w; s.height = h; s.seed = seed;
    s.eyes = {e};
    return s;
}

}  // namespace

TEST("detector: sub-pixel pupil centre, radius and glint") {
    const SynthEye e = eyeAt({100.37, 80.61});
    const Image img = renderScene(oneEye(e));
    DetectorParams p;
    const EyeMeasurement m = detectEye(img.view(), {104, 84}, p);
    REQUIRE(m.valid);
    CHECK_NEAR(m.pupil.x, e.pupil.x, 0.2);
    CHECK_NEAR(m.pupil.y, e.pupil.y, 0.2);
    CHECK_NEAR(m.radius, e.pupilR, 0.6);
    CHECK(m.confidence > 0.8);
    REQUIRE(m.hasGlint);
    CHECK_NEAR(m.glint.x, e.pupil.x + 3.0, 0.3);
    CHECK_NEAR(m.glint.y, e.pupil.y + 4.0, 0.3);
}

TEST("detector: small pupil (far user) still sub-pixel") {
    const SynthEye e = eyeAt({90.8, 70.2}, 3.5, {1.5, 2.0});
    const Image img = renderScene(oneEye(e, 180, 140, 3));
    DetectorParams p;
    const EyeMeasurement m = detectEye(img.view(), {92, 71}, p);
    REQUIRE(m.valid);
    CHECK_NEAR(m.pupil.x, e.pupil.x, 0.35);
    CHECK_NEAR(m.pupil.y, e.pupil.y, 0.35);
}

TEST("detector: glint sitting on the pupil edge does not bias the centre") {
    const SynthEye e = eyeAt({100.5, 80.25}, 8, {8.0, 0.0});
    const Image img = renderScene(oneEye(e, 200, 160, 11));
    DetectorParams p;
    const EyeMeasurement m = detectEye(img.view(), {100, 80}, p);
    REQUIRE(m.valid);
    CHECK_NEAR(m.pupil.x, e.pupil.x, 0.4);
    CHECK_NEAR(m.pupil.y, e.pupil.y, 0.4);
}

TEST("detector: upper eyelid covering the top of the pupil") {
    SynthEye e = eyeAt({100.2, 80.7}, 9, {2.0, 5.0});
    e.lidTop = 5.5;                                      // top ~3.5 px of the pupil hidden
    const Image img = renderScene(oneEye(e, 200, 160, 5));
    DetectorParams p;
    const EyeMeasurement m = detectEye(img.view(), {100, 82}, p);
    REQUIRE(m.valid);
    CHECK_NEAR(m.pupil.x, e.pupil.x, 0.5);
    CHECK_NEAR(m.pupil.y, e.pupil.y, 1.2);
}

TEST("detector: no eye in the window -> invalid") {
    SynthScene s;
    s.width = 200; s.height = 160;
    const Image img = renderScene(s);
    DetectorParams p;
    CHECK(!detectEye(img.view(), {100, 80}, p).valid);
}

TEST("detector: big glasses reflection is not a glint, dark brow is not a pupil") {
    SynthEye e = eyeAt({100.3, 90.4});
    SynthScene s = oneEye(e, 200, 180, 9);
    s.spots.push_back({{128, 70}, 9, 252});             // glasses reflection
    s.spots.push_back({{100, 38}, 7, 30});              // eyebrow / lash shadow, darker than skin
    const Image img = renderScene(s);
    DetectorParams p;
    const EyeMeasurement m = detectEye(img.view(), {102, 92}, p);
    REQUIRE(m.valid);
    CHECK_NEAR(m.pupil.x, e.pupil.x, 0.3);
    CHECK_NEAR(m.pupil.y, e.pupil.y, 0.3);
    REQUIRE(m.hasGlint);
    CHECK_NEAR(m.glint.x, e.pupil.x + 3.0, 0.4);
    CHECK_NEAR(m.glint.y, e.pupil.y + 4.0, 0.4);
}

// A face-sized frame (half of the 2304x1296 camera mode) with distractors.
static SynthScene faceScene(Vec2 left, Vec2 right, uint32_t seed = 21) {
    SynthScene s;
    s.width = 1152; s.height = 648; s.seed = seed;
    s.eyes = {eyeAt(left, 6, {2, 3}), eyeAt(right, 6, {2, 3})};
    s.spots.push_back({{150, 120}, 14, 250});            // lamp
    s.spots.push_back({{900, 500}, 1.5, 255});           // tiny bright dot on skin, no pupil
    s.spots.push_back({{300, 450}, 9, 20});              // dark blob without a glint
    return s;
}

TEST("finder: picks the two eyes, ignores lamp, dot and dark blob") {
    const Vec2 L(420.4, 300.7), R(611.2, 304.1);
    const Image img = renderScene(faceScene(L, R));
    TrackerParams p;
    const EyePair pr = pickEyePair(findEyeCandidates(img.view(), p), img.width, p);
    REQUIRE(pr.found);
    CHECK_NEAR(pr.eye[0].pupil.x, L.x, 0.4);
    CHECK_NEAR(pr.eye[0].pupil.y, L.y, 0.4);
    CHECK_NEAR(pr.eye[1].pupil.x, R.x, 0.4);
    CHECK_NEAR(pr.eye[1].pupil.y, R.y, 0.4);
}

TEST("tracker: acquires, follows a saccade, survives a blink, keeps slot order") {
    TrackerParams p;
    EyeTracker tr(p);
    Vec2 L(420.4, 300.7), R(611.2, 304.1);
    FrameEyes fe = tr.process(renderScene(faceScene(L, R, 1)).view());
    REQUIRE(fe.eye[0].valid && fe.eye[1].valid);
    CHECK_NEAR(fe.ipdPx, dist(L, R), 0.5);

    L = L + Vec2(6.3, -2.1); R = R + Vec2(6.1, -2.2);      // saccade + head drift
    fe = tr.process(renderScene(faceScene(L, R, 2)).view());
    REQUIRE(fe.eye[0].valid && fe.eye[1].valid);
    CHECK_NEAR(fe.eye[0].pupil.x, L.x, 0.4);
    CHECK_NEAR(fe.eye[1].pupil.x, R.x, 0.4);

    SynthScene closed = faceScene(L, R, 3);                // blink: pupils gone
    for (auto& e : closed.eyes) { e.pupilLevel = e.irisLevel = e.scleraLevel = closed.skinLevel; e.glints.clear(); }
    for (int i = 0; i < 4; ++i) {
        fe = tr.process(renderScene(closed).view());
        CHECK(!fe.eye[0].valid && !fe.eye[1].valid);
        CHECK(fe.tracked[0] && fe.tracked[1]);             // windows kept through the blink
    }
    fe = tr.process(renderScene(faceScene(L, R, 4)).view());
    REQUIRE(fe.eye[0].valid && fe.eye[1].valid);
    CHECK(fe.eye[0].pupil.x < fe.eye[1].pupil.x);
    CHECK(!fe.searched);
}
