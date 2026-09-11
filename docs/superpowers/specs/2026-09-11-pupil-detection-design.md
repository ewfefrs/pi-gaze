# Pi-Gaze — Module 1: Pupil Detection (design)

- **Date:** 2026-09-11
- **Status:** approved (design), pending spec review
- **Author:** Claude (with user)

## Goal

Produce a reliable **per-frame pupil center (x, y) for each eye** from a remote,
monitor-mounted IR camera, verifiable **headless over SSH**. This is the input
foundation for later gaze mapping — it does not itself move a cursor.

## Context & constraints

- **Hardware:** Raspberry Pi 5, Camera Module 3 **NoIR**, IR illuminators, mounted
  on the bar at the monitor's top edge, tilted down at the user's face.
- **Geometry:** *remote* — the whole face is in frame at ~40–70 cm. So we must
  localize face → eyes **before** looking for the pupil (the pupil is only a few
  pixels wide at this distance).
- **Illumination:** IR (NoIR sensor + IR LEDs) gives a **dark-pupil** signal and
  bright **corneal glints** from the LEDs.
- **Software:** C++17 (real-time perf), OpenCV, Raspberry Pi OS Lite 64-bit
  (Bookworm), headless. Pi 5 camera stack is **libcamera** (not legacy V4L2).

## Scope

**In:** camera capture; face + eye localization; pupil center per eye; glint
position; per-result confidence; headless verification (annotated frame dump +
optional MJPEG stream); a small config file; FPS logging.

**Out (later modules, not now):** gaze mapping & calibration; CH9329 USB-HID mouse
output; control-loop smoothing/filtering; multi-user; auto IR power control.

## Architecture (units, each independently testable)

1. **Camera** — opens the Camera Module 3 through libcamera and yields grayscale
   `cv::Mat` frames. Path: OpenCV `VideoCapture` with the **GStreamer**
   `libcamerasrc` pipeline (`libcamerasrc ! video/x-raw ! videoconvert ! appsink`),
   the clean C++ route on Pi 5. Exposes manual exposure/gain for IR tuning.
2. **FaceEyes** — runs OpenCV's built-in **YuNet** DNN face detector
   (`cv::FaceDetectorYN`, mainline objdetect since 4.5.4). YuNet returns the face
   box **plus 5 landmarks including both eye centers**, so we get eye ROIs with no
   separate eye cascade and no heavy landmark model. Emits `std::vector<Eye>`.
3. **PupilDetector** — within an eye ROI: normalize → adaptive threshold on the
   **dark-pupil** region → contour extraction → filter by area/circularity →
   **ellipse fit** for a sub-pixel center; separately take the brightest spot as
   the IR **glint**. Emits a `PupilResult`.
4. **Telemetry/Verify** — a `PupilResult` sink: structured log (coords, radius,
   confidence, FPS), annotated-frame writer (JPEG to disk), and an optional MJPEG
   HTTP server so the result can be watched in a browser over SSH.
5. **Config** — a small file (key=value or minimal JSON): camera resolution/fps,
   exposure/gain, threshold/ROI-scale, output mode. Sane defaults if absent.
6. **App loop** — wires Camera → FaceEyes → PupilDetector → Telemetry; clean
   startup/shutdown, signal handling.

## Interfaces (sketch)

```cpp
struct PupilResult {
    cv::Point2f center;        // in full-frame coordinates
    float       radius;        // px (ellipse minor/major avg)
    float       confidence;    // 0..1
    std::optional<cv::Point2f> glint;
    bool        valid;
};
struct Eye { cv::Rect roi; cv::Point2f center; bool isLeft; };

class Camera        { public: bool open(const Config&); bool next(cv::Mat& gray); };
class FaceEyes      { public: bool load(const std::string& model); std::vector<Eye> detect(const cv::Mat& gray); };
class PupilDetector { public: PupilResult detect(const cv::Mat& gray, const Eye& eye); };
```

## Data flow

`frame → FaceEyes.detect → per-eye ROI → PupilDetector.detect → PupilResult[] → Telemetry`

## Error handling

- Camera/GStreamer open fails → clear message, non-zero exit.
- No face this frame → emit "no face", keep running (don't crash/stall).
- Low-confidence or no pupil → `valid=false`; never emit a garbage coordinate.
- Missing config → defaults; malformed config → warn + defaults for bad keys.

## Verification (headless, over SSH)

- **Golden test:** a handful of recorded IR eye frames are checked into
  `test/data/` with hand-labeled pupil centers; the detector must land within a
  pixel tolerance. Runs in CI-style `ctest` with no camera attached.
- **Live check:** annotated-frame dump + FPS counter; optional MJPEG to eyeball it
  in a browser.
- **Success criteria:** on a cooperative user at ~60 cm, pupil center jitter
  **< ~3 px** frame-to-frame, **≥ 15 FPS** at the chosen resolution, and graceful
  "no face"/`valid=false` handling when the eye is occluded or absent.

## Build & deploy

- **CMake** project under `pi-gaze/software/`. Dependencies from apt on Bookworm:
  `libopencv-dev` (has DNN + GStreamer), `gstreamer1.0-libcamera`,
  `libcamera-dev`. YuNet `.onnx` model vendored under `software/models/`.
- Build **on the Pi** (needs a **≥16 GB** card — the current 3.7 GB card fits the
  OS but not OpenCV + toolchain) or cross-compile. First bring-up can validate raw
  capture with `rpicam-hello` before OpenCV is present.

## Assumptions / open items (safe defaults chosen)

- Resolution/FPS start at **1280×720 @ 30**; revisit if the pupil is too small.
- Detect **both** eyes; downstream picks the higher-confidence one.
- IR exposure/gain set **manually** in config for v1 (no auto control yet).
