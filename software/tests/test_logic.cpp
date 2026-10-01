#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "pigaze/button_logic.hpp"
#include "pigaze/calibration.hpp"
#include "pigaze/ch9329.hpp"
#include "pigaze/config.hpp"
#include "pigaze/controller.hpp"
#include "pigaze/debugview.hpp"
#include "pigaze/filter.hpp"
#include "pigaze/gaze.hpp"
#include "testing.hpp"

using namespace pigaze;

namespace {

// A simulated user: each eye's feature is a (slightly nonlinear) function of the screen point.
Vec2 userFeature(int eye, const Vec2& s) {
    const double x = s.x - 0.5, y = s.y - 0.5;
    const double ox = eye == 0 ? -0.4 : 0.3, oy = eye == 0 ? 0.2 : 0.25;
    return {ox + 9.0 * x + 0.8 * y + 1.5 * x * x, oy + 0.6 * x + 6.5 * y - 1.2 * y * y + 0.7 * x * y};
}

FrameEyes eyesLookingAt(const Vec2& s, bool visible = true) {
    FrameEyes e;
    e.ipdPx = 200;
    for (int k = 0; k < 2; ++k) {
        EyeMeasurement& m = e.eye[k];
        m.valid = visible;
        m.hasGlint = visible;
        m.glint = {500.0 + 200 * k, 300.0};
        m.pupil = m.glint + userFeature(k, s) * (e.ipdPx / 100.0);
        m.radius = 6;
        m.confidence = 0.9;
        e.tracked[k] = true;
    }
    return e;
}

struct Recorder : CursorOutput {
    std::vector<Vec2> moves;
    int clicks = 0;
    void moveTo(const Vec2& n) override { moves.push_back(n); }
    void click() override { ++clicks; }
};

struct FakeLink : ByteLink {
    std::vector<std::vector<uint8_t>> frames;
    size_t pending = 0;
    bool write(const uint8_t* d, size_t n) override { frames.emplace_back(d, d + n); return true; }
    int read(uint8_t*, size_t) override { return 0; }
    size_t queued() const override { return pending; }
};

}  // namespace

TEST("gaze: polynomial map recovers a quadratic mapping, survives save/load") {
    std::vector<Vec2> in, out;
    for (double y = -4; y <= 4; y += 4)
        for (double x = -4; x <= 4; x += 4) {
            in.push_back({x, y});
            out.push_back({0.5 + 0.08 * x + 0.01 * y + 0.004 * x * y + 0.003 * x * x, 0.5 + 0.1 * y - 0.002 * x + 0.005 * y * y});
        }
    PolyMap m;
    REQUIRE(m.fit(in, out, 1e-9));
    CHECK(m.terms() == 6);
    const Vec2 p = m.apply({1.7, -2.3});
    CHECK_NEAR(p.x, 0.5 + 0.08 * 1.7 + 0.01 * -2.3 + 0.004 * 1.7 * -2.3 + 0.003 * 1.7 * 1.7, 1e-6);
    CHECK_NEAR(p.y, 0.5 + 0.1 * -2.3 - 0.002 * 1.7 + 0.005 * 2.3 * 2.3, 1e-6);

    GazeModel g;
    g.eye[1] = m;
    g.rms[1] = 0.012;
    const std::string path = "test_calibration.tmp";
    REQUIRE(g.save(path));
    GazeModel h;
    REQUIRE(h.load(path));
    std::remove(path.c_str());
    CHECK(!h.eye[0].ok());
    CHECK(h.eye[1].ok());
    CHECK_NEAR(h.rms[1], 0.012, 1e-12);
    CHECK_NEAR(h.eye[1].apply({1.7, -2.3}).x, p.x, 1e-9);
}

TEST("gaze: feature = (pupil - glint) normalised by the pupil distance") {
    EyeMeasurement m;
    m.valid = m.hasGlint = true;
    m.pupil = {110, 204};
    m.glint = {106, 206};
    const GazeFeature f = makeFeature(m, 200);
    REQUIRE(f.valid);
    CHECK_NEAR(f.v.x, 2.0, 1e-12);
    CHECK_NEAR(f.v.y, -1.0, 1e-12);
    m.hasGlint = false;
    CHECK(!makeFeature(m, 200).valid);
}

TEST("calibration: 9 points with a simulated user give an accurate model") {
    CalibParams p;
    CalibrationSession s(p, 10.0);
    double t = 10.0;
    int frames = 0;
    Vec2 target = CalibrationSession::points(p.margin)[0];
    while (!s.finished() && frames < 2000) {
        GazeFeature f[2];
        for (int k = 0; k < 2; ++k) { f[k].valid = true; f[k].v = userFeature(k, target); }
        target = s.update(t, f);
        t += 1.0 / 30;
        ++frames;
    }
    REQUIRE(s.finished());
    CHECK_NEAR(t - 10.0, p.readyS + 9 * (p.settleS + p.collectS), 0.1);
    GazeModel m;
    std::string report;
    REQUIRE(s.result(m, report));
    CHECK(m.eye[0].ok() && m.eye[1].ok());
    CHECK(m.rms[0] < 0.01 && m.rms[1] < 0.01);
    for (const Vec2 q : {Vec2(0.3, 0.7), Vec2(0.8, 0.2), Vec2(0.55, 0.45)}) {
        GazeFeature f[2];
        for (int k = 0; k < 2; ++k) { f[k].valid = true; f[k].v = userFeature(k, q); }
        Vec2 g;
        REQUIRE(m.predict(f, g));
        CHECK_NEAR(g.x, q.x, 0.02);
        CHECK_NEAR(g.y, q.y, 0.02);
    }
}

TEST("calibration: no eyes during the session -> rejected with a reason") {
    CalibParams p;
    CalibrationSession s(p, 0.0);
    GazeFeature f[2];
    for (double t = 0; !s.finished(); t += 1.0 / 30) s.update(t, f);
    GazeModel m;
    std::string report;
    CHECK(!s.result(m, report));
    CHECK(report.find("not enough") != std::string::npos);
}

TEST("filter: One Euro smooths jitter but follows a jump") {
    OneEuro f(0.6, 2.0, 1.0);
    double t = 0, out = 0, maxDev = 0;
    for (int i = 0; i < 90; ++i, t += 1.0 / 30) {
        out = f.filter(0.5 + (i % 2 ? 0.01 : -0.01), t);
        if (i > 30) maxDev = std::max(maxDev, std::fabs(out - 0.5));
    }
    CHECK(maxDev < 0.004);                               // +-1 % jitter -> < 0.4 %
    for (int i = 0; i < 15; ++i, t += 1.0 / 30) out = f.filter(0.9, t);
    CHECK_NEAR(out, 0.9, 0.02);                           // saccade followed within 0.5 s
}

TEST("filter: dwell fires once per stay and re-arms after leaving") {
    Dwell d(1.0, 0.02);
    int fired = 0;
    double t = 0;
    for (int i = 0; i < 90; ++i, t += 1.0 / 30) fired += d.update({0.5, 0.5}, t);
    CHECK(fired == 1);
    for (int i = 0; i < 5; ++i, t += 1.0 / 30) fired += d.update({0.8, 0.5}, t);
    for (int i = 0; i < 40; ++i, t += 1.0 / 30) fired += d.update({0.5, 0.5}, t);
    CHECK(fired == 2);
}

TEST("ch9329: frame bytes, checksum, info reply") {
    const std::vector<uint8_t> want = {0x57, 0xAB, 0x00, 0x04, 0x07, 0x02, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x1F};
    CHECK(ch9329::mouseAbs(0, 2048, 2048) == want);
    CHECK(ch9329::getInfo() == std::vector<uint8_t>({0x57, 0xAB, 0x00, 0x01, 0x00, 0x03}));
    CHECK(ch9329::toAbs(1.0) == 4095);
    CHECK(ch9329::toAbs(-0.2) == 0);
    std::vector<uint8_t> rx = {0x00, 0x57, 0xAB, 0x00, 0x81, 0x08, 0x30, 0x01, 0x00, 0, 0, 0, 0, 0};
    unsigned sum = 0;
    for (size_t i = 1; i < rx.size(); ++i) sum += rx[i];
    rx.push_back(static_cast<uint8_t>(sum));
    ch9329::Info info;
    REQUIRE(ch9329::parseInfo(rx, info));
    CHECK(info.version == 0x30);
    CHECK(info.usbConnected);
    rx.back() ^= 0xFF;                                   // corrupt checksum
    CHECK(!ch9329::parseInfo(rx, info));
}

TEST("ch9329: HidMouse skips repeats, drops moves on a busy line, always clicks") {
    FakeLink link;
    HidMouse mouse(link);
    mouse.moveTo({0.5, 0.5});
    mouse.moveTo({0.5, 0.5});
    CHECK(link.frames.size() == 1);
    link.pending = 40;
    mouse.moveTo({0.7, 0.5});
    CHECK(link.frames.size() == 1);
    CHECK(mouse.dropped() == 1);
    mouse.click();
    REQUIRE(link.frames.size() == 3);
    CHECK(link.frames[1][6] == 0x01);                    // button down
    CHECK(link.frames[2][6] == 0x00);                    // button up, same place
    CHECK(link.frames[1][7] == link.frames[0][7] && link.frames[1][8] == link.frames[0][8]);
}

TEST("config: values, comments, warnings") {
    AppConfig c;
    std::vector<std::string> w;
    parseConfig("# comment\nwidth = 1536\nheight=864  # inline\nfps = 60\nlens_position = 2.5\n"
                "dwell_click = yes\nhid_device = /dev/ttyAMA1\nbogus = 1\nfps = fast\n",
                c, w);
    CHECK(c.width == 1536 && c.height == 864 && c.fps == 60);
    CHECK_NEAR(c.lensPosition, 2.5, 1e-12);
    CHECK(c.ctl.dwellClick);
    CHECK(c.hidDevice == "/dev/ttyAMA1");
    CHECK(w.size() == 2);                                // unknown key + bad value
    CHECK(c.ctl.calibFile == "/var/lib/pigaze/calibration.txt");
}

TEST("controller: auto-calibrates, then the cursor follows the gaze") {
    Recorder out;
    ControllerParams p;
    Controller c(p, out);
    double t = 0;
    const double dt = 1.0 / 30;
    for (int i = 0; i < 30; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.5, 0.5}));
    CHECK(c.mode() == Mode::NeedCalibration);
    CHECK(out.moves.empty());                            // never touches the mouse uncalibrated
    for (int i = 0; i < 40; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.5, 0.5}));
    REQUIRE(c.mode() == Mode::Calibrating);
    Vec2 look = {0.5, 0.5};
    for (int i = 0; i < 1000 && c.mode() == Mode::Calibrating; ++i, t += dt) {
        c.onFrame(t, eyesLookingAt(look));
        if (!out.moves.empty()) look = out.moves.back();  // the user follows the cursor
    }
    REQUIRE(c.mode() == Mode::Tracking);
    CHECK(c.status().calibrated);
    for (int i = 0; i < 60; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.25, 0.8}));
    REQUIRE(!out.moves.empty());
    CHECK_NEAR(out.moves.back().x, 0.25, 0.02);
    CHECK_NEAR(out.moves.back().y, 0.8, 0.02);

    c.command(Command::Toggle, t);                       // pause: mouse left alone
    CHECK(c.mode() == Mode::Paused);
    const size_t n = out.moves.size();
    for (int i = 0; i < 10; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.7, 0.3}));
    CHECK(out.moves.size() == n);
    CHECK(c.status().gazeValid);                         // but gaze is still reported
    c.command(Command::Toggle, t);
    CHECK(c.mode() == Mode::Tracking);
    for (int i = 0; i < 5; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.7, 0.3}, false));
    CHECK(out.moves.size() == n);                        // blink / no eyes: cursor holds still
}

TEST("controller: failed calibration keeps state and does not loop") {
    Recorder out;
    ControllerParams p;
    Controller c(p, out);
    double t = 0;
    const double dt = 1.0 / 30;
    c.command(Command::Calibrate, t);
    REQUIRE(c.mode() == Mode::Calibrating);
    for (int i = 0; i < 800 && c.mode() == Mode::Calibrating; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.5, 0.5}, false));
    CHECK(c.mode() == Mode::NeedCalibration);
    CHECK(c.status().lastCalibReport.find("not enough") != std::string::npos);
    for (int i = 0; i < 200; ++i, t += dt) c.onFrame(t, eyesLookingAt({0.5, 0.5}));
    CHECK(c.mode() == Mode::NeedCalibration || c.mode() == Mode::Calibrating);
}

TEST("button: single, double, slow double, long press") {
    PressClassifier b;
    b.press(0.0);
    CHECK(b.release(0.1) == ButtonEvent::None);
    CHECK(b.tick(0.3) == ButtonEvent::None);             // still waiting for a second press
    CHECK(b.tick(0.6) == ButtonEvent::Single);
    CHECK(b.tick(0.7) == ButtonEvent::None);

    b.press(1.0);
    b.release(1.1);
    b.press(1.3);
    CHECK(b.tick(1.6) == ButtonEvent::None);             // second press held: no single yet
    CHECK(b.release(1.7) == ButtonEvent::Double);
    CHECK(b.tick(2.5) == ButtonEvent::None);

    b.press(3.0);
    b.release(3.1);
    b.press(3.6);                                        // 0.5 s later: two singles
    CHECK(b.release(3.7) == ButtonEvent::Single);
    CHECK(b.tick(4.2) == ButtonEvent::Single);

    b.press(5.0);
    CHECK(b.release(7.0) == ButtonEvent::None);          // long press = power off, ignored
    CHECK(b.tick(8.0) == ButtonEvent::None);
}

TEST("debugview: BMP header and pixel order") {
    RgbImage im(3, 2);
    im.set(0, 0, 255, 0, 0);                             // top-left red
    const std::vector<uint8_t> b = encodeBmp(im);
    REQUIRE(b.size() == 54 + 12 * 2);
    CHECK(b[0] == 'B' && b[1] == 'M');
    CHECK(b[18] == 3 && b[22] == 2 && b[28] == 24);
    const size_t topRow = 54 + 12;                       // rows are stored bottom-up
    CHECK(b[topRow] == 0 && b[topRow + 1] == 0 && b[topRow + 2] == 255);
    Image frame(640, 360, 100);
    FrameEyes e;
    const RgbImage d = renderDebug(frame.view(), e, 2, 3);
    CHECK(d.width == 392 && d.height == 180 + 8 + 192);
}
