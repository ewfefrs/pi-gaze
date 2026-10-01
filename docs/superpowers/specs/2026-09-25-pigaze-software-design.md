# Pi-Gaze — software v1 (design)

- **Date:** 2026-09-25
- **Status:** approved by delegation ("продумай логику сам, доводи до ума")
- **Supersedes:** `2026-09-11-pupil-detection-design.md` (OpenCV + YuNet) — see "Why not OpenCV".

## Goal

Eyes move the PC cursor. The Pi watches the user's eyes through the IR camera on the
bar, estimates where on the screen they look, and moves the PC's mouse through the
CH9329 (USB HID). No software on the PC.

## Why not OpenCV (change from the 09-11 spec)

The only SD card is **3.7 GB**; Pi OS Lite 64-bit (Trixie) takes 3.06 GB, leaving ~0.9 GB.
OpenCV-dev + GStreamer + toolchain does not fit. The IR setup gives a much cheaper eye
finder than a face DNN: **corneal glints** (reflections of our own IR LEDs) are the
brightest tiny spots in the frame and each sits right next to a **dark pupil**. So the
whole pipeline is plain C++17 with no third-party libraries:

- camera frames come from `rpicam-vid --codec yuv420 -o -` (preinstalled rpicam-apps),
  we read the Y plane from the pipe;
- detection, calibration, filtering, HID, debug web page — our own code;
- the portable core (everything except camera/serial/button/http) builds and is
  unit-tested on the Windows PC with MSVC, and again on the Pi with g++ before install.

## Pipeline

```
rpicam-vid (Y plane) -> EyeTracker -> features -> GazeModel -> OneEuro -> Controller -> CH9329 -> PC cursor
                         |  finder: glint + dark pupil, pairs by IPD
                         |  tracker: per-eye ROI around last pupil
```

1. **EyeDetector** (per eye ROI): median/threshold -> glint blobs (small, bright) ->
   inpaint glints -> darkest seed (with distance penalty) -> region grow with an adaptive
   pupil/iris threshold (2 passes) -> moments -> radial edge points -> ellipse fit
   (fallback circle, fallback centroid) -> confidence. Glint = intensity-weighted centroid
   of the small bright blobs near the pupil (the two LED reflections are < 3 px apart at
   60 cm, so they are treated as one reference point).
2. **EyeFinder** (full frame, only when not tracking): bright small blobs with local
   contrast -> run EyeDetector around each -> dedupe -> choose the best **pair** whose
   distance fits the IPD range and is roughly horizontal. Initial acquisition requires a
   pair (so slot 0 = image-left eye, slot 1 = image-right eye is unambiguous).
3. **EyeTracker**: per slot ROI tracking; blink tolerance (`lost_frames`); re-acquires a
   single lost eye from the other eye + last inter-eye offset; full search when both lost.
   Keeps a smoothed IPD in px (head-distance scale).
4. **Feature** per eye: `(pupil - glint) * 100 / IPD_px` — pupil-centre/corneal-reflection
   vector normalised by head distance.
5. **GazeModel**: per eye 2nd-order polynomial (6 terms per axis, ridge LS; affine if < 6
   points), inputs normalised. Prediction = confidence-free mean of the valid eyes.
   Saved to `/var/lib/pigaze/calibration.txt`.
6. **Calibration without a PC app**: the Pi itself puts the cursor (CH9329 absolute mode)
   on 9 points (3x3, 10 % margin), wiggles it briefly to catch the eye, waits for the
   saccade to settle, collects ~1 s of features, takes per-point medians, fits, checks
   RMS. Success -> cursor draws a small circle; failure -> cursor shakes, old calibration
   is kept.
7. **Filtering**: One Euro filter (low jitter on fixations, low lag on saccades); cursor
   held still when no valid gaze (blink, looking away). Optional dwell click (off).
8. **Controller** modes: `NeedCalibration -> Calibrating -> Tracking <-> Paused`.
   Inputs: Pi 5 **power button** (logind told to ignore it): single press = pause/resume
   (or start calibration if none), double press = recalibrate; long press stays
   "power off" (logind `HandlePowerKeyLongPress=poweroff`). Also a FIFO
   `/run/pigaze/control` (`pigaze-ctl calibrate|toggle|pause|resume`) and the debug page.
   Auto-calibration on first run once eyes are seen for 2 s.
9. **CH9329**: UART0 on GPIO14/15 (`/dev/ttyAMA0`, 9600 8N1, `dtparam=uart0=on`),
   absolute mouse frames `57 AB 00 04 07 02 btn xl xh yl yh wh sum`, 0..4095 range.
   Moves are dropped (not queued) if the UART still has > 2 frames pending -> no lag build-up.
10. **Debug page** (`http_bind:http_port`, default `127.0.0.1:8080`, reach via
    `ssh -L 8080:localhost:8080`): live downscaled IR frame + eye zooms with the pupil/glint
    overlay (BMP, no encoder needed), JSON status, Calibrate / Pause buttons. Localhost by
    default because it shows the user's face.

## Files

```
software/
  include/pigaze/*.hpp      src/core/*.cpp   (portable, unit-tested on PC and Pi)
  src/linux/*.cpp  src/main.cpp              (rpicam pipe, termios, input, http, fifo)
  tests/*.cpp                                (own tiny harness, no downloads)
  config/pigaze.conf   deploy/ (systemd units, installer, logind drop-in, pigaze-ctl)
  CMakeLists.txt  build_tests_windows.cmd
```

## Deploy

SD card: Pi OS Lite 64-bit Trixie; cloud-init `user-data` creates user `pigaze`
(SSH **key only**, no password), hostname `pigaze`, enables SSH; `config.txt` gets
`dtparam=uart0=on`; the source is copied to the boot partition. On first boot a oneshot
service (retried every boot until it succeeds, needs network for `apt install g++`)
builds + runs the tests on the Pi, installs `/usr/local/bin/pigaze`, the config, the
service and the logind drop-in, then reboots once. Wi-Fi: the user types SSID/password
into `network-config` themselves, or plugs Ethernet.

## Error handling

- Camera missing / rpicam-vid exits -> logged, restarted every 3 s; service keeps running.
- CH9329 missing -> logged once; tracking still runs (debug page works), retried every 5 s.
- No eyes / low confidence -> no cursor movement, never a garbage coordinate.
- Bad config keys -> warning + default.

## Verification

- PC: `software/build_tests_windows.cmd` (MSVC) — detector accuracy on synthetic IR eyes
  (sub-pixel centre, glint on the pupil edge, eyelid, noise, no-eye rejection), finder
  on a synthetic face with distractors, tracker through a blink, polynomial fit,
  calibration session, One Euro, dwell, CH9329 bytes, config, controller modes, BMP.
- Pi: installer compiles and runs the same tests before installing; `pigaze --selftest`
  runs the synthetic scene through the live binary; `pigaze --snapshot f.pgm` grabs a
  real frame for tuning; debug page for live checks.

## Known limits (v1)

- Accuracy is that of a single-camera PCCR tracker: expect ~1–2 deg (1–2 cm at 60 cm);
  moving the head a lot calls for recalibration (double press).
- Absolute HID maps to the primary monitor on Windows.
- Glasses reflections: large bright blobs are ignored as glints, but can hide the pupil.
