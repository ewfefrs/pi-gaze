# Pupil Detection (Module 1) Implementation Plan

> **SUPERSEDED (2026-09-25):** not executed. Replaced by the dependency-free implementation in
> `software/` — see `docs/superpowers/specs/2026-09-25-pigaze-software-design.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect a per-frame pupil center (x, y) for each eye from a remote, monitor-mounted IR camera, verifiable headless over SSH.

**Architecture:** A small C++17/CMake project under `pi-gaze/software/`. A `pigaze_core` library exposes independent units — Config, PupilDetector, Telemetry, FaceEyes, Camera — wired by a thin `pigaze` app. The pipeline is Camera → FaceEyes (YuNet) → PupilDetector (IR dark-pupil ellipse fit) → Telemetry. Camera-independent units are built and unit-tested first on any Linux dev box; the camera unit is brought up on the Pi.

**Tech Stack:** C++17, CMake, OpenCV (`libopencv-dev`, uses `objdetect`/`FaceDetectorYN`, `imgproc`, `dnn`, GStreamer capture), doctest (vendored single header), libcamera via OpenCV's GStreamer `libcamerasrc` backend.

## Global Constraints

- Language **C++17**; build **CMake ≥ 3.16**; compiler **g++ ≥ 10** or clang.
- **OpenCV ≥ 4.5.4** with `objdetect` (for `cv::FaceDetectorYN`), `imgproc`, `dnn`, and **GStreamer** capture — install via `apt install libopencv-dev` on Debian **Bookworm** (the Pi) or Ubuntu (a **WSL2** dev box; native Windows C++ OpenCV is out of scope).
- Test framework: **doctest**, vendored at `software/third_party/doctest.h`; one test binary `pigaze_tests` registered with **CTest**.
- All pupil/glint coordinates are in **full-frame pixel space** unless a signature says ROI-local.
- Camera capture uses **libcamera** through OpenCV `VideoCapture(pipeline, cv::CAP_GSTREAMER)` — never legacy V4L2.
- **TDD**: every unit lands with its failing test first. **Commit after every green step.**
- Build/run on the Pi needs a **≥ 16 GB** SD card (OpenCV + toolchain); the 3.7 GB card is OS-only.

## File Structure

```
software/
  CMakeLists.txt              # top-level: core lib + app + tests
  third_party/doctest.h       # vendored single-header test framework
  include/pigaze/
    types.hpp                 # PupilResult, Eye (POD, no logic)
    config.hpp                # Config struct + load()
    pupil_detector.hpp        # PupilDetector
    telemetry.hpp             # Telemetry (log/annotate/writeFrame)
    face_eyes.hpp             # FaceEyes (YuNet)
    camera.hpp                # Camera (libcamera/GStreamer)
  src/
    config.cpp
    pupil_detector.cpp
    telemetry.cpp
    face_eyes.cpp
    camera.cpp
    main.cpp                  # app loop
  models/                     # face_detection_yunet_2023mar.onnx (vendored)
  test/
    test_types.cpp
    test_config.cpp
    test_pupil_detector.cpp
    test_telemetry.cpp
    test_golden.cpp
    test_face_eyes.cpp
    fixtures.hpp              # synthetic-image helpers shared by tests
    data/                     # golden frames + labels, a test face image
  config.example.ini
```

Tasks 1–5 build and test with **no camera** (any Linux/WSL2). Task 6 needs the YuNet model + a face image (still no camera). Tasks 7–8 need the Pi + camera.

---

### Task 1: Project scaffold, types, and a green CTest

**Files:**
- Create: `software/CMakeLists.txt`, `software/third_party/doctest.h` (download), `software/include/pigaze/types.hpp`, `software/test/test_types.cpp`
- Test: `software/test/test_types.cpp`

**Interfaces:**
- Produces: `pigaze::PupilResult { cv::Point2f center; float radius; float confidence; std::optional<cv::Point2f> glint; bool valid; }` and `pigaze::Eye { cv::Rect roi; cv::Point2f center; bool isLeft; }`.

- [ ] **Step 1: Vendor doctest**

Run: `curl -L -o software/third_party/doctest.h https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h`
Expected: file ~230 KB exists.

- [ ] **Step 2: Write the failing test**

`software/test/test_types.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/types.hpp"

TEST_CASE("PupilResult default is invalid") {
    pigaze::PupilResult r;
    CHECK(r.valid == false);
    CHECK_FALSE(r.glint.has_value());
}
TEST_CASE("Eye holds an ROI and a side flag") {
    pigaze::Eye e{ cv::Rect(1, 2, 3, 4), cv::Point2f(2.5f, 4.0f), true };
    CHECK(e.roi.width == 3);
    CHECK(e.isLeft);
}
```

- [ ] **Step 3: Write `CMakeLists.txt` (core lib + tests)**

`software/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
project(pigaze LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenCV REQUIRED)
enable_testing()

add_library(pigaze_core)           # sources added by later tasks
target_include_directories(pigaze_core PUBLIC include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(pigaze_core PUBLIC ${OpenCV_LIBS})
set_target_properties(pigaze_core PROPERTIES LINKER_LANGUAGE CXX)

add_executable(pigaze_tests
    test/test_types.cpp)
target_include_directories(pigaze_tests PRIVATE third_party include)
target_link_libraries(pigaze_tests PRIVATE pigaze_core)
add_test(NAME pigaze_tests COMMAND pigaze_tests)
```
Add a doctest main once, at the top of `test/test_types.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
```
(placed above the existing `#include "doctest.h"`).

`pigaze_core` needs at least one source to link; add a placeholder now and replace in Task 2:
```cmake
target_sources(pigaze_core PRIVATE src/placeholder.cpp)
```
Create `software/src/placeholder.cpp` with `namespace pigaze { int _link_stub = 0; }`.

- [ ] **Step 4: Write `types.hpp`**

`software/include/pigaze/types.hpp`:
```cpp
#pragma once
#include <optional>
#include <opencv2/core.hpp>
namespace pigaze {
struct PupilResult {
    cv::Point2f center{};
    float radius{0.f};
    float confidence{0.f};
    std::optional<cv::Point2f> glint{};
    bool valid{false};
};
struct Eye {
    cv::Rect roi{};
    cv::Point2f center{};
    bool isLeft{false};
};
}  // namespace pigaze
```

- [ ] **Step 5: Configure, build, run — expect PASS**

Run: `cmake -S software -B software/build && cmake --build software/build -j && ctest --test-dir software/build --output-on-failure`
Expected: `pigaze_tests` PASS (2 test cases).

- [ ] **Step 6: Commit**
```bash
git add software/ && git commit -m "feat(sw): scaffold CMake+doctest, core types"
```

---

### Task 2: Config loader

**Files:**
- Create: `software/include/pigaze/config.hpp`, `software/src/config.cpp`, `software/config.example.ini`, `software/test/test_config.cpp`
- Modify: `software/CMakeLists.txt` (add `src/config.cpp` to `pigaze_core`, delete the placeholder; add `test/test_config.cpp` to `pigaze_tests`)

**Interfaces:**
- Consumes: nothing.
- Produces: `struct Config { int width=1280, height=720, fps=30; double exposure=-1, gain=-1; int pupilBlockSize=31; double pupilC=10; double roiScale=1.6; std::string outputMode="jpeg"; std::string modelPath="models/face_detection_yunet_2023mar.onnx"; };` and `Config load(const std::string& path);` (missing file → all defaults; unknown/malformed lines → warn to stderr, keep defaults).

- [ ] **Step 1: Write the failing test**

`software/test/test_config.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/config.hpp"
#include <fstream>

TEST_CASE("missing file yields defaults") {
    auto c = pigaze::load("definitely_absent.ini");
    CHECK(c.width == 1280);
    CHECK(c.fps == 30);
}
TEST_CASE("known keys override defaults; junk ignored") {
    const char* p = "test_cfg_tmp.ini";
    std::ofstream(p) << "width=640\nfps=60\nnonsense line\nroiScale=2.0\n";
    auto c = pigaze::load(p);
    CHECK(c.width == 640);
    CHECK(c.fps == 60);
    CHECK(c.roiScale == doctest::Approx(2.0));
    CHECK(c.height == 720);  // untouched default
    std::remove(p);
}
```

- [ ] **Step 2: Run to verify it fails**
Run: `cmake --build software/build -j` — expect compile error (`config.hpp` missing).

- [ ] **Step 3: Implement `config.hpp` + `config.cpp`**

`software/include/pigaze/config.hpp`:
```cpp
#pragma once
#include <string>
namespace pigaze {
struct Config {
    int width = 1280, height = 720, fps = 30;
    double exposure = -1, gain = -1;   // -1 = auto/driver default
    int pupilBlockSize = 31;           // adaptiveThreshold block (odd)
    double pupilC = 10;                // adaptiveThreshold constant
    double roiScale = 1.6;             // eye ROI size vs inter-landmark
    std::string outputMode = "jpeg";   // "jpeg" | "none" | "mjpeg"
    std::string modelPath = "models/face_detection_yunet_2023mar.onnx";
};
Config load(const std::string& path);
}  // namespace pigaze
```

`software/src/config.cpp`:
```cpp
#include "pigaze/config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
namespace pigaze {
Config load(const std::string& path) {
    Config c; std::ifstream f(path);
    if (!f) return c;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (line.empty() || line[0] == '#') continue;
        if (eq == std::string::npos) { std::cerr << "config: skip '" << line << "'\n"; continue; }
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        try {
            if      (k == "width")         c.width = std::stoi(v);
            else if (k == "height")        c.height = std::stoi(v);
            else if (k == "fps")           c.fps = std::stoi(v);
            else if (k == "exposure")      c.exposure = std::stod(v);
            else if (k == "gain")          c.gain = std::stod(v);
            else if (k == "pupilBlockSize")c.pupilBlockSize = std::stoi(v);
            else if (k == "pupilC")        c.pupilC = std::stod(v);
            else if (k == "roiScale")      c.roiScale = std::stod(v);
            else if (k == "outputMode")    c.outputMode = v;
            else if (k == "modelPath")     c.modelPath = v;
            else std::cerr << "config: unknown key '" << k << "'\n";
        } catch (...) { std::cerr << "config: bad value for '" << k << "'\n"; }
    }
    return c;
}
}  // namespace pigaze
```
Update CMake: remove `src/placeholder.cpp`, delete that file, add `src/config.cpp` to `pigaze_core` and `test/test_config.cpp` to `pigaze_tests`. Write `config.example.ini` with the default keys commented.

- [ ] **Step 4: Build + run — expect PASS**
Run: `cmake -S software -B software/build && cmake --build software/build -j && ctest --test-dir software/build --output-on-failure`

- [ ] **Step 5: Commit**
```bash
git add software/ && git commit -m "feat(sw): config loader with defaults"
```

---

### Task 3: PupilDetector (IR dark-pupil) — the core CV

**Files:**
- Create: `software/include/pigaze/pupil_detector.hpp`, `software/src/pupil_detector.cpp`, `software/test/fixtures.hpp`, `software/test/test_pupil_detector.cpp`
- Modify: `software/CMakeLists.txt`

**Interfaces:**
- Consumes: `pigaze::Eye`, `pigaze::PupilResult`, `pigaze::Config`.
- Produces: `class PupilDetector { public: explicit PupilDetector(const Config&); PupilResult detect(const cv::Mat& gray, const Eye& eye) const; };` — `gray` is full-frame 8-bit; returns center/glint in **full-frame** coords; `valid=false` when no plausible pupil.

- [ ] **Step 1: Write shared fixtures + failing test**

`software/test/fixtures.hpp`:
```cpp
#pragma once
#include <opencv2/imgproc.hpp>
#include "pigaze/types.hpp"
// Gray 8-bit frame with a dark pupil disk (+optional bright glint) inside an eye ROI.
inline cv::Mat makeEyeFrame(cv::Rect roi, cv::Point2f pupil, int pupilR,
                            int bg=140, bool glint=false, cv::Point2f gpt={}) {
    cv::Mat img(200, 320, CV_8UC1, cv::Scalar(bg));
    cv::rectangle(img, roi, cv::Scalar(bg-20), cv::FILLED);           // eye socket, slightly darker
    cv::circle(img, pupil, pupilR, cv::Scalar(15), cv::FILLED);       // dark pupil
    if (glint) cv::circle(img, gpt, 2, cv::Scalar(255), cv::FILLED);  // IR glint
    return img;
}
```

`software/test/test_pupil_detector.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/pupil_detector.hpp"
#include "fixtures.hpp"

TEST_CASE("finds a centered dark pupil within 1.5 px") {
    cv::Rect roi(120, 70, 80, 60);
    cv::Point2f pupil(160, 100);
    auto img = makeEyeFrame(roi, pupil, 10);
    pigaze::Config cfg;
    pigaze::PupilDetector det(cfg);
    auto r = det.detect(img, pigaze::Eye{roi, {160,100}, true});
    REQUIRE(r.valid);
    CHECK(cv::norm(r.center - pupil) < 1.5);
}
TEST_CASE("no pupil -> invalid, no garbage") {
    cv::Rect roi(120, 70, 80, 60);
    cv::Mat flat(200, 320, CV_8UC1, cv::Scalar(140));
    pigaze::PupilDetector det(pigaze::Config{});
    auto r = det.detect(flat, pigaze::Eye{roi, {160,100}, true});
    CHECK_FALSE(r.valid);
}
TEST_CASE("glint is the brightest spot in the ROI") {
    cv::Rect roi(120, 70, 80, 60);
    auto img = makeEyeFrame(roi, {160,100}, 10, 140, true, {168,96});
    auto r = pigaze::PupilDetector(pigaze::Config{}).detect(img, pigaze::Eye{roi,{160,100},true});
    REQUIRE(r.glint.has_value());
    CHECK(cv::norm(*r.glint - cv::Point2f(168,96)) < 2.0);
}
```

- [ ] **Step 2: Run to verify it fails** — expect compile error (no `pupil_detector.hpp`).

- [ ] **Step 3: Implement**

`software/include/pigaze/pupil_detector.hpp`:
```cpp
#pragma once
#include "pigaze/types.hpp"
#include "pigaze/config.hpp"
namespace pigaze {
class PupilDetector {
public:
    explicit PupilDetector(const Config& cfg) : cfg_(cfg) {}
    PupilResult detect(const cv::Mat& gray, const Eye& eye) const;
private:
    Config cfg_;
};
}  // namespace pigaze
```

`software/src/pupil_detector.cpp`:
```cpp
#include "pigaze/pupil_detector.hpp"
#include <opencv2/imgproc.hpp>
#include <vector>
namespace pigaze {
PupilResult PupilDetector::detect(const cv::Mat& gray, const Eye& eye) const {
    PupilResult out;
    cv::Rect roi = eye.roi & cv::Rect(0, 0, gray.cols, gray.rows);
    if (roi.area() <= 0) return out;
    cv::Mat sub = gray(roi), blur, bin;
    cv::GaussianBlur(sub, blur, cv::Size(5, 5), 0);
    int bs = cfg_.pupilBlockSize | 1;  // force odd
    cv::adaptiveThreshold(blur, bin, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                          cv::THRESH_BINARY_INV, bs, cfg_.pupilC);  // dark -> white
    std::vector<std::vector<cv::Point>> cs;
    cv::findContours(bin, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double best = 0; int bi = -1;
    for (int i = 0; i < (int)cs.size(); ++i) {
        double a = cv::contourArea(cs[i]);
        if (a < 20 || cs[i].size() < 5) continue;
        double per = cv::arcLength(cs[i], true);
        double circ = per > 0 ? 4 * CV_PI * a / (per * per) : 0;  // 1.0 = perfect circle
        if (circ < 0.5) continue;
        double score = a * circ;
        if (score > best) { best = score; bi = i; }
    }
    if (bi >= 0) {
        cv::RotatedRect e = cv::fitEllipse(cs[bi]);
        out.center = e.center + cv::Point2f((float)roi.x, (float)roi.y);
        out.radius = (e.size.width + e.size.height) / 4.f;
        out.confidence = (float)std::min(1.0, best / (roi.area() * 0.2));
        out.valid = true;
    }
    double mn, mx; cv::Point mnL, mxL;
    cv::minMaxLoc(sub, &mn, &mx, &mnL, &mxL);
    if (mx > 220) out.glint = cv::Point2f(mxL.x + roi.x, mxL.y + roi.y);
    return out;
}
}  // namespace pigaze
```
Add `src/pupil_detector.cpp` to `pigaze_core`; add `test/test_pupil_detector.cpp` (and `-I test` for `fixtures.hpp`) to `pigaze_tests`.

- [ ] **Step 4: Build + run — expect PASS** (`ctest --test-dir software/build --output-on-failure`)

- [ ] **Step 5: Commit**
```bash
git add software/ && git commit -m "feat(sw): IR dark-pupil detector (ellipse fit + glint)"
```

---

### Task 4: Telemetry (log + annotate + JPEG dump)

**Files:**
- Create: `software/include/pigaze/telemetry.hpp`, `software/src/telemetry.cpp`, `software/test/test_telemetry.cpp`
- Modify: `software/CMakeLists.txt`

**Interfaces:**
- Consumes: `PupilResult`, `Eye`.
- Produces: `class Telemetry { public: static std::string logLine(int frame, const std::vector<PupilResult>&); static void annotate(cv::Mat& bgr, const std::vector<PupilResult>&); static bool writeJpeg(const std::string& path, const cv::Mat& img); };`

- [ ] **Step 1: Write the failing test**

`software/test/test_telemetry.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/telemetry.hpp"

TEST_CASE("logLine reports coords and validity") {
    pigaze::PupilResult a; a.valid = true; a.center = {12.0f, 34.0f}; a.confidence = 0.9f;
    auto s = pigaze::Telemetry::logLine(7, {a});
    CHECK(s.find("frame=7") != std::string::npos);
    CHECK(s.find("12") != std::string::npos);
    CHECK(s.find("34") != std::string::npos);
}
TEST_CASE("annotate marks the pupil pixel") {
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(0,0,0));
    pigaze::PupilResult a; a.valid = true; a.center = {50,50}; a.radius = 6;
    pigaze::Telemetry::annotate(img, {a});
    CHECK(cv::countNonZero(img.reshape(1)) > 0);  // something was drawn
}
```

- [ ] **Step 2: Run to verify it fails.**

- [ ] **Step 3: Implement**

`software/include/pigaze/telemetry.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>
#include "pigaze/types.hpp"
namespace pigaze {
class Telemetry {
public:
    static std::string logLine(int frame, const std::vector<PupilResult>& rs);
    static void annotate(cv::Mat& bgr, const std::vector<PupilResult>& rs);
    static bool writeJpeg(const std::string& path, const cv::Mat& img);
};
}  // namespace pigaze
```

`software/src/telemetry.cpp`:
```cpp
#include "pigaze/telemetry.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
namespace pigaze {
std::string Telemetry::logLine(int frame, const std::vector<PupilResult>& rs) {
    std::ostringstream o; o << "frame=" << frame;
    for (size_t i = 0; i < rs.size(); ++i) {
        o << " eye" << i << "=";
        if (rs[i].valid) o << "(" << rs[i].center.x << "," << rs[i].center.y
                           << ",c" << rs[i].confidence << ")";
        else o << "invalid";
    }
    return o.str();
}
void Telemetry::annotate(cv::Mat& bgr, const std::vector<PupilResult>& rs) {
    for (const auto& r : rs) {
        if (!r.valid) continue;
        cv::circle(bgr, r.center, std::max(2, (int)r.radius), cv::Scalar(0,255,0), 1);
        cv::drawMarker(bgr, r.center, cv::Scalar(0,0,255), cv::MARKER_CROSS, 6, 1);
        if (r.glint) cv::circle(bgr, *r.glint, 2, cv::Scalar(255,0,0), 1);
    }
}
bool Telemetry::writeJpeg(const std::string& path, const cv::Mat& img) {
    return cv::imwrite(path, img);
}
}  // namespace pigaze
```
Add to CMake (`src/telemetry.cpp`, `test/test_telemetry.cpp`).

- [ ] **Step 4: Build + run — expect PASS.**
- [ ] **Step 5: Commit**
```bash
git add software/ && git commit -m "feat(sw): telemetry log/annotate/jpeg"
```

---

### Task 5: Golden verification harness (synthetic frames)

**Files:**
- Create: `software/test/test_golden.cpp`
- Modify: `software/CMakeLists.txt`

**Interfaces:**
- Consumes: `PupilDetector`, `makeEyeFrame` (fixtures), `Eye`.
- Produces: nothing (test-only). Establishes the tolerance gate the spec requires; real recorded frames are dropped into `test/data/` during Pi bring-up (Task 7) and appended here.

- [ ] **Step 1: Write the golden test (a sweep of known centers)**

`software/test/test_golden.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/pupil_detector.hpp"
#include "fixtures.hpp"

TEST_CASE("golden: pupil center within 2 px across a grid of positions") {
    pigaze::PupilDetector det(pigaze::Config{});
    cv::Rect roi(110, 60, 100, 80);
    int hits = 0, total = 0;
    for (int dx = -20; dx <= 20; dx += 10)
        for (int dy = -15; dy <= 15; dy += 15) {
            cv::Point2f p(160 + dx, 100 + dy);
            auto img = makeEyeFrame(roi, p, 9, 140, true, {p.x+6, p.y-6});
            auto r = det.detect(img, pigaze::Eye{roi, {160,100}, true});
            ++total;
            if (r.valid && cv::norm(r.center - p) < 2.0) ++hits;
        }
    CHECK(hits == total);  // 15/15 must pass
}
```

- [ ] **Step 2: Build + run — expect PASS** (the detector from Task 3 must clear the whole grid).
If any position fails, fix the detector (Task 3), do not loosen the tolerance without cause.

- [ ] **Step 3: Commit**
```bash
git add software/ && git commit -m "test(sw): golden pupil-center tolerance gate"
```

---

### Task 6: FaceEyes (YuNet) — face box + eye ROIs

**Files:**
- Create: `software/include/pigaze/face_eyes.hpp`, `software/src/face_eyes.cpp`, `software/test/test_face_eyes.cpp`, `software/models/` (model), `software/test/data/face_sample.jpg` (a freely-licensed, front-facing face photo the user supplies or a CC0 image)
- Modify: `software/CMakeLists.txt`

**Interfaces:**
- Consumes: `Config`, `Eye`.
- Produces: `class FaceEyes { public: bool load(const std::string& modelPath, cv::Size input); std::vector<Eye> detect(const cv::Mat& gray, double roiScale) const; };` — returns up to 2 `Eye`s (left/right) with `roi` a square of side `roiScale × inter-eye-distance/…` centered on each YuNet eye landmark.

- [ ] **Step 1: Fetch the model**

Run: `curl -L -o software/models/face_detection_yunet_2023mar.onnx https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx`
Expected: ~230 KB ONNX file.

- [ ] **Step 2: Write the failing test** (needs `data/face_sample.jpg`; if absent, the test is skipped via `doctest` `WARN` + early return)

`software/test/test_face_eyes.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/face_eyes.hpp"
#include <opencv2/imgcodecs.hpp>

TEST_CASE("YuNet finds a face and two eyes in a sample image") {
    cv::Mat gray = cv::imread("test/data/face_sample.jpg", cv::IMREAD_GRAYSCALE);
    if (gray.empty()) { WARN("no face_sample.jpg; skipping"); return; }
    pigaze::FaceEyes fe;
    REQUIRE(fe.load("models/face_detection_yunet_2023mar.onnx", gray.size()));
    auto eyes = fe.detect(gray, 1.6);
    CHECK(eyes.size() == 2);
    for (auto& e : eyes) CHECK(e.roi.area() > 0);
}
```

- [ ] **Step 3: Implement with `cv::FaceDetectorYN`**

`software/src/face_eyes.cpp` (key logic):
```cpp
#include "pigaze/face_eyes.hpp"
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
namespace pigaze {
struct FaceEyes::Impl { cv::Ptr<cv::FaceDetectorYN> det; cv::Size in; };
bool FaceEyes::load(const std::string& m, cv::Size input) {
    impl_ = std::make_shared<Impl>();
    impl_->in = input;
    impl_->det = cv::FaceDetectorYN::create(m, "", input, 0.7f, 0.3f, 5000);
    return impl_->det != nullptr;
}
std::vector<Eye> FaceEyes::detect(const cv::Mat& gray, double roiScale) const {
    std::vector<Eye> out;
    if (!impl_ || !impl_->det) return out;
    cv::Mat bgr; cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
    cv::Mat faces; impl_->det->detect(bgr, faces);
    if (faces.rows < 1) return out;
    // Row 0 = highest score. Cols 4..13 = 5 landmarks (x,y): [0]=right eye [1]=left eye.
    const float* f = faces.ptr<float>(0);
    cv::Point2f re(f[4], f[5]), le(f[6], f[7]);
    float d = (float)cv::norm(re - le);
    int s = std::max(16, (int)(roiScale * d * 0.5f));
    auto mk = [&](cv::Point2f c, bool left) {
        cv::Rect r(int(c.x - s/2), int(c.y - s/2), s, s);
        out.push_back(Eye{ r & cv::Rect(0,0,gray.cols,gray.rows), c, left });
    };
    mk(le, true); mk(re, false);
    return out;
}
}  // namespace pigaze
```
`face_eyes.hpp` declares the class with a `std::shared_ptr<Impl> impl_;` (pimpl to keep OpenCV objdetect out of the header). Add sources/tests to CMake. Copy `models/` and `test/data/` next to the test binary or run tests from `software/` (working dir matters for the relative paths).

- [ ] **Step 4: Build + run — expect PASS (or SKIP if no sample image).**
- [ ] **Step 5: Commit**
```bash
git add software/ && git commit -m "feat(sw): YuNet face/eye localization"
```

---

### Task 7: Camera (libcamera via GStreamer) — Pi bring-up

**Files:**
- Create: `software/include/pigaze/camera.hpp`, `software/src/camera.cpp`, `software/test/test_camera.cpp`
- Modify: `software/CMakeLists.txt`

**Interfaces:**
- Consumes: `Config`.
- Produces: `class Camera { public: bool open(const Config&); bool next(cv::Mat& gray); std::string pipeline(const Config&) const; };` — `pipeline()` builds the GStreamer string (pure, unit-testable); `open()`/`next()` need real hardware.

- [ ] **Step 1: Pre-flight on the Pi (manual)**
Run on the Pi: `rpicam-hello --timeout 2000` — expect the camera preview/log (proves libcamera sees Camera Module 3). If this fails, stop and fix the camera before continuing.

- [ ] **Step 2: Write the failing (pure) test for the pipeline string**

`software/test/test_camera.cpp`:
```cpp
#include "doctest.h"
#include "pigaze/camera.hpp"
TEST_CASE("pipeline embeds libcamerasrc and the requested size/fps") {
    pigaze::Config c; c.width = 1280; c.height = 720; c.fps = 30;
    auto p = pigaze::Camera().pipeline(c);
    CHECK(p.find("libcamerasrc") != std::string::npos);
    CHECK(p.find("1280") != std::string::npos);
    CHECK(p.find("appsink") != std::string::npos);
}
```

- [ ] **Step 3: Implement**

`software/src/camera.cpp` (key parts):
```cpp
#include "pigaze/camera.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
namespace pigaze {
std::string Camera::pipeline(const Config& c) const {
    std::ostringstream o;
    o << "libcamerasrc ! video/x-raw,width=" << c.width
      << ",height=" << c.height << ",framerate=" << c.fps << "/1"
      << " ! videoconvert ! video/x-raw,format=BGR ! appsink drop=true max-buffers=2";
    return o.str();
}
bool Camera::open(const Config& c) {
    cap_.open(pipeline(c), cv::CAP_GSTREAMER);
    return cap_.isOpened();
}
bool Camera::next(cv::Mat& gray) {
    cv::Mat bgr;
    if (!cap_.read(bgr) || bgr.empty()) return false;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    return true;
}
}  // namespace pigaze
```
`camera.hpp` holds a `cv::VideoCapture cap_;`. Add a **guarded** integration test (runs only if `PIGAZE_CAMERA=1`) that opens and grabs one non-empty frame. Add to CMake.

- [ ] **Step 4: Build + run** — pure pipeline test PASS on any box; on the Pi with camera: `PIGAZE_CAMERA=1 ctest --test-dir software/build -R camera --output-on-failure`.
- [ ] **Step 5: Commit**
```bash
git add software/ && git commit -m "feat(sw): libcamera/GStreamer capture"
```

---

### Task 8: App loop (`main`) + example config

**Files:**
- Create: `software/src/main.cpp`
- Modify: `software/CMakeLists.txt` (add `pigaze` executable = `main.cpp` + `pigaze_core`)

**Interfaces:**
- Consumes: `Config load()`, `Camera`, `FaceEyes`, `PupilDetector`, `Telemetry`.
- Produces: the `pigaze` binary.

- [ ] **Step 1: Implement the loop**

`software/src/main.cpp`:
```cpp
#include "pigaze/config.hpp"
#include "pigaze/camera.hpp"
#include "pigaze/face_eyes.hpp"
#include "pigaze/pupil_detector.hpp"
#include "pigaze/telemetry.hpp"
#include <opencv2/imgproc.hpp>
#include <csignal>
#include <iostream>
#include <chrono>
static volatile std::sig_atomic_t g_stop = 0;
int main(int argc, char** argv) {
    std::signal(SIGINT, [](int){ g_stop = 1; });
    auto cfg = pigaze::load(argc > 1 ? argv[1] : "config.ini");
    pigaze::Camera cam;
    if (!cam.open(cfg)) { std::cerr << "camera open failed\n"; return 1; }
    pigaze::FaceEyes fe;
    if (!fe.load(cfg.modelPath, {cfg.width, cfg.height}))
        { std::cerr << "model load failed: " << cfg.modelPath << "\n"; return 2; }
    pigaze::PupilDetector det(cfg);
    cv::Mat gray; int frame = 0; auto t0 = std::chrono::steady_clock::now();
    while (!g_stop) {
        if (!cam.next(gray)) continue;
        auto eyes = fe.detect(gray, cfg.roiScale);
        std::vector<pigaze::PupilResult> rs;
        for (auto& e : eyes) rs.push_back(det.detect(gray, e));
        std::cout << pigaze::Telemetry::logLine(frame, rs) << "\n";
        if (cfg.outputMode == "jpeg" && frame % 15 == 0) {
            cv::Mat bgr; cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
            pigaze::Telemetry::annotate(bgr, rs);
            pigaze::Telemetry::writeJpeg("last_frame.jpg", bgr);
        }
        ++frame;
    }
    auto dt = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
    std::cerr << "avg fps=" << (frame/dt) << "\n";
    return 0;
}
```
Add the `pigaze` executable to CMake.

- [ ] **Step 2: Build — expect success** (`cmake --build software/build -j`).
- [ ] **Step 3: Run on the Pi (manual smoke)**
Run: `cd software && ./build/pigaze config.example.ini` — sit ~60 cm away; `scp` or view `last_frame.jpg` and confirm the green pupil markers land on your pupils. Confirm `avg fps ≥ 15`.
- [ ] **Step 4: Commit**
```bash
git add software/ && git commit -m "feat(sw): app loop wiring camera->faceeyes->pupil->telemetry"
```

---

## Self-Review

**Spec coverage:** capture (T7), face+eye localization YuNet (T6), pupil center per eye (T3), glint (T3), confidence (T3), headless verify: annotated JPEG (T4) + golden gate (T5) + FPS (T8), config (T2), error handling: camera fail (T7/T8), no face (T6 returns empty → T8 emits invalid), low-confidence invalid (T3). MJPEG output mode is declared in Config but implemented as JPEG dump in v1; MJPEG is explicitly out of the v1 build (spec lists it "optional") — noted, not a gap.

**Placeholder scan:** no TBD/TODO; every code step has real code; tests carry real assertions.

**Type consistency:** `PupilResult`/`Eye` (T1) used unchanged in T3/T4/T6/T8; `Config` fields (T2) referenced by exact name in T3 (`pupilBlockSize`,`pupilC`), T6 (`roiScale`,`modelPath`), T7 (`width`/`height`/`fps`), T8. `PupilDetector::detect(gray, eye)`, `FaceEyes::load/detect`, `Camera::open/next/pipeline`, `Telemetry::logLine/annotate/writeJpeg` names match across tasks.
