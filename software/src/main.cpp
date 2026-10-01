// Pi-Gaze: IR camera -> eye tracker -> gaze model -> CH9329 -> PC cursor.
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "pigaze/ch9329.hpp"
#include "pigaze/config.hpp"
#include "pigaze/controller.hpp"
#include "pigaze/debugview.hpp"
#include "pigaze/platform.hpp"
#include "pigaze/synth.hpp"
#include "pigaze/tracker.hpp"

using namespace pigaze;

namespace {

const char* kVersion = "pigaze 1.0";
std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

void logLine(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stdout, fmt, ap);
    va_end(ap);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') { o += '\\'; o += ch; }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += ch;
    }
    return o;
}

std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

bool parseCommand(const std::string& s, Command& c) {
    if (s == "calibrate") c = Command::Calibrate;
    else if (s == "toggle") c = Command::Toggle;
    else if (s == "pause") c = Command::Pause;
    else if (s == "resume") c = Command::Resume;
    else return false;
    return true;
}

class CommandQueue {
public:
    void push(Command c) { std::lock_guard<std::mutex> lk(m_); q_.push_back(c); }
    std::vector<Command> take() { std::lock_guard<std::mutex> lk(m_); std::vector<Command> r; r.swap(q_); return r; }

private:
    std::mutex m_;
    std::vector<Command> q_;
};

// HID when the UART is open, otherwise the cursor is simply not moved.
struct CursorSwitch : CursorOutput {
    HidMouse* hid = nullptr;
    void moveTo(const Vec2& n) override { if (hid) hid->moveTo(n); }
    void click() override { if (hid) hid->click(); }
};

// State shared with the HTTP thread.
struct Shared {
    std::mutex m;
    std::string status = "{}";
    Frame frame;
    FrameEyes eyes;
    bool haveFrame = false;
    double lastFrameRequest = -1e9;
};

const char kPage[] = R"HTML(<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pi-Gaze</title>
<style>
body{font-family:system-ui,sans-serif;background:#111;color:#ddd;margin:16px}
button{font-size:16px;margin:4px 8px 4px 0;padding:8px 14px;border-radius:6px;border:1px solid #555;background:#222;color:#eee}
img{max-width:100%;border:1px solid #333;image-rendering:pixelated}
pre{background:#1a1a1a;padding:8px;overflow:auto;font-size:12px}
#line{font-size:18px}
</style></head><body>
<h2>Pi-Gaze</h2>
<div><button onclick="cmd('calibrate')">Калибровка</button><button onclick="cmd('toggle')">Пауза / продолжить</button></div>
<p id="line">…</p>
<img id="f" alt="ИК-кадр">
<p style="color:#888">Жёлтое — окна глаз, красное — зрачок, зелёное — блики ИК-диодов.</p>
<pre id="s"></pre>
<script>
const names={'need-calibration':'нужна калибровка','calibrating':'калибровка','tracking':'слежение','paused':'пауза'};
async function cmd(c){await fetch('/cmd/'+c,{method:'POST'});}
async function tick(){try{const s=await (await fetch('/status')).json();
document.getElementById('s').textContent=JSON.stringify(s,null,2);
let l='режим: '+(names[s.mode]||s.mode)+' · '+s.fps.toFixed(1)+' к/с';
if(s.mode==='calibrating')l+=' · точка '+s.calib.point+'/'+s.calib.points;
if(s.gaze.valid)l+=' · взгляд '+(s.gaze.x*100).toFixed(0)+'%, '+(s.gaze.y*100).toFixed(0)+'%';
document.getElementById('line').textContent=l;}catch(e){}}
function frame(){const i=document.getElementById('f');const n=new Image();
n.onload=()=>{i.src=n.src;setTimeout(frame,150)};n.onerror=()=>setTimeout(frame,1000);n.src='/frame.bmp?'+Date.now();}
setInterval(tick,500);tick();frame();
</script></body></html>
)HTML";

SynthEye synthEye(Vec2 c, double r) {
    SynthEye e;
    e.pupil = c;
    e.pupilR = r;
    e.irisR = std::max(2.4 * r, 16.0);
    e.eyeW = e.irisR * 1.9;
    e.eyeH = e.irisR * 0.95;
    e.glints = {c + Vec2(0.7, 3), c + Vec2(3.3, 3)};
    return e;
}

// Camera-free check of the live binary: synthetic face -> tracker, accuracy and timing.
int selftest(const AppConfig& cfg) {
    SynthScene s;
    s.width = cfg.width;
    s.height = cfg.height;
    const Vec2 L(cfg.width * 0.42 + 0.37, cfg.height * 0.46 + 0.61), R(L.x + cfg.width * 0.08 + 0.2, L.y + 2.3);
    s.eyes = {synthEye(L, 7), synthEye(R, 7)};
    s.spots.push_back({{cfg.width * 0.1, cfg.height * 0.2}, 14, 250});
    logLine("selftest: rendering a %dx%d synthetic face...", s.width, s.height);
    const Image img = renderScene(s);
    EyeTracker tr(cfg.tracker);
    double t0 = monotonicSeconds();
    FrameEyes fe = tr.process(img.view());
    const double searchMs = (monotonicSeconds() - t0) * 1000;
    if (!fe.eye[0].valid || !fe.eye[1].valid) { logLine("selftest: FAIL - eyes not found"); return 1; }
    t0 = monotonicSeconds();
    const int n = 50;
    for (int i = 0; i < n; ++i) fe = tr.process(img.view());
    const double trackMs = (monotonicSeconds() - t0) * 1000 / n;
    const double e0 = dist(fe.eye[0].pupil, L), e1 = dist(fe.eye[1].pupil, R);
    logLine("selftest: eye0 error %.2f px, eye1 error %.2f px, ipd %.1f px (true %.1f)", e0, e1, fe.ipdPx, dist(L, R));
    logLine("selftest: full-frame search %.1f ms, tracking %.2f ms/frame", searchMs, trackMs);
    const bool ok = e0 < 0.5 && e1 < 0.5 && fe.eye[0].hasGlint && fe.eye[1].hasGlint;
    logLine("selftest: %s", ok ? "OK" : "FAIL");
    return ok ? 0 : 1;
}

int snapshot(const AppConfig& cfg, const std::string& path) {
    RpicamSource cam(cfg);
    std::string err;
    if (!cam.start(err)) { logLine("camera: %s", err.c_str()); return 1; }
    Frame f;
    int got = 0;
    const double deadline = monotonicSeconds() + 15;
    while (got < 15 && monotonicSeconds() < deadline && !g_stop)   // let exposure settle
        if (cam.next(f, 500)) ++got;
    cam.stop();
    if (got == 0) { logLine("camera: no frames (is the camera connected? try: rpicam-hello --list-cameras)"); return 1; }
    if (!writePgm(path, f.view())) { logLine("cannot write %s", path.c_str()); return 1; }
    logLine("saved %dx%d frame to %s", f.width, f.height, path.c_str());
    return 0;
}

void usage() {
    std::puts("usage: pigaze [--config FILE] [--no-hid] [--calibrate]\n"
              "       pigaze --selftest | --snapshot FILE.pgm | --version\n"
              "  --config FILE    settings (default /etc/pigaze/pigaze.conf)\n"
              "  --no-hid         track and show on the debug page, never move the mouse\n"
              "  --calibrate      start with a calibration\n"
              "  --selftest       synthetic-image check of the tracker (no camera)\n"
              "  --snapshot FILE  save one camera frame (PGM) for tuning");
}

}  // namespace

int main(int argc, char** argv) {
    std::string cfgPath = "/etc/pigaze/pigaze.conf", snapPath;
    bool selftestMode = false, noHid = false, calibrateNow = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--config" && i + 1 < argc) cfgPath = argv[++i];
        else if (a == "--snapshot" && i + 1 < argc) snapPath = argv[++i];
        else if (a == "--selftest") selftestMode = true;
        else if (a == "--no-hid") noHid = true;
        else if (a == "--calibrate") calibrateNow = true;
        else if (a == "--version") { std::puts(kVersion); return 0; }
        else { usage(); return a == "--help" ? 0 : 2; }
    }
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    signal(SIGPIPE, SIG_IGN);

    AppConfig cfg;
    std::vector<std::string> warnings;
    if (!loadConfigFile(cfgPath, cfg, warnings)) logLine("config %s not found: defaults", cfgPath.c_str());
    for (const std::string& w : warnings) logLine("config: %s", w.c_str());
    if (selftestMode) return selftest(cfg);
    if (!snapPath.empty()) return snapshot(cfg, snapPath);

    logLine("%s starting (%dx%d@%d)", kVersion, cfg.width, cfg.height, cfg.fps);
    EyeTracker tracker(cfg.tracker);
    SerialLink serial;
    HidMouse hid(serial);
    CursorSwitch cursor;
    const bool hidWanted = cfg.hidEnabled && !noHid;
    bool hidUsb = false, hidErrLogged = false;
    double nextHidTry = 0;

    Controller ctl(cfg.ctl, cursor);
    if (!ctl.loadCalibration()) logLine("no calibration yet: %s", cfg.ctl.autoCalibrate ? "it starts once your eyes are seen" : "double-press the power button");
    CommandQueue cmds;
    if (calibrateNow) cmds.push(Command::Calibrate);

    std::string err;
    PowerButton button;
    if (button.start([&cmds](ButtonEvent e) { cmds.push(e == ButtonEvent::Single ? Command::Toggle : Command::Calibrate); }, err))
        logLine("power button: single press = pause/resume, double press = calibrate");
    else logLine("power button: %s", err.c_str());
    ControlFifo fifo;
    if (!fifo.start("/run/pigaze/control", [&cmds](const std::string& line) {
            Command c;
            if (parseCommand(line, c)) cmds.push(c);
            else logLine("control: unknown command '%s'", line.c_str());
        }, err))
        logLine("control fifo: %s", err.c_str());

    Shared shared;
    DebugHttp http;
    const int shrink = std::max(1, (cfg.width + 799) / 800);
    if (cfg.httpPort > 0) {
        const bool ok = http.start(cfg.httpBind, cfg.httpPort,
            [&](const std::string& method, const std::string& path, std::string& type, std::string& body) {
                if (path == "/") { type = "text/html; charset=utf-8"; body = kPage; return true; }
                if (path == "/status") {
                    std::lock_guard<std::mutex> lk(shared.m);
                    type = "application/json";
                    body = shared.status;
                    return true;
                }
                if (path == "/frame.bmp") {
                    std::lock_guard<std::mutex> lk(shared.m);
                    shared.lastFrameRequest = monotonicSeconds();
                    if (!shared.haveFrame) return false;
                    const std::vector<uint8_t> bmp = encodeBmp(renderDebug(shared.frame.view(), shared.eyes, shrink, 3));
                    type = "image/bmp";
                    body.assign(bmp.begin(), bmp.end());
                    return true;
                }
                Command c;
                if (method == "POST" && path.rfind("/cmd/", 0) == 0 && parseCommand(path.substr(5), c)) {
                    cmds.push(c);
                    type = "application/json";
                    body = "{\"ok\":true}";
                    return true;
                }
                return false;
            },
            err);
        if (ok) logLine("debug page: http://%s:%d/", cfg.httpBind.c_str(), cfg.httpPort);
        else logLine("debug page: %s", err.c_str());
    }

    RpicamSource cam(cfg);
    double nextCamTry = 0, lastLog = monotonicSeconds(), lastStatus = 0, lastSnap = 0, fps = 0, prevT = 0;
    long nFrames = 0, nBoth = 0, nAny = 0;
    Frame frame;
    while (!g_stop) {
        const double now = monotonicSeconds();
        if (hidWanted && !serial.isOpen() && now >= nextHidTry) {
            nextHidTry = now + 5;
            if (serial.open(cfg.hidDevice, cfg.hidBaud, err)) {
                const auto q = ch9329::getInfo();
                serial.write(q.data(), q.size());
                usleep(150000);
                std::vector<uint8_t> rx(64);
                rx.resize(static_cast<size_t>(std::max(0, serial.read(rx.data(), rx.size()))));
                ch9329::Info info;
                if (ch9329::parseInfo(rx, info)) {
                    hidUsb = info.usbConnected;
                    logLine("CH9329 v%02x on %s, USB %s", info.version, cfg.hidDevice.c_str(),
                         info.usbConnected ? "connected to the PC" : "NOT connected to the PC");
                } else {
                    logLine("CH9329: no answer on %s @%d (check TX<->RX crossing, GND, 3.3 V levels)", cfg.hidDevice.c_str(), cfg.hidBaud);
                }
                cursor.hid = &hid;
                hidErrLogged = false;
            } else if (!hidErrLogged) {
                logLine("CH9329: %s (retrying every 5 s)", err.c_str());
                hidErrLogged = true;
            }
        }
        if (!cam.running() && now >= nextCamTry) {
            nextCamTry = now + 5;
            if (cam.start(err)) {
                std::string line;
                for (const std::string& a : cam.args()) line += a + " ";
                logLine("camera: %s", line.c_str());
            } else {
                logLine("camera: %s", err.c_str());
            }
        }
        if (!cam.next(frame, 200)) {
            if (!cam.running()) usleep(200000);            // camera down: wait, do not spin
            for (Command c : cmds.take()) ctl.command(c, now);
            for (const std::string& l : ctl.takeLog()) logLine("%s", l.c_str());
            continue;
        }

        const FrameEyes fe = tracker.process(frame.view());
        for (Command c : cmds.take()) ctl.command(c, frame.t);
        ctl.onFrame(frame.t, fe);
        for (const std::string& l : ctl.takeLog()) logLine("%s", l.c_str());

        ++nFrames;
        nBoth += fe.eye[0].valid && fe.eye[1].valid;
        nAny += fe.eye[0].valid || fe.eye[1].valid;
        if (prevT > 0 && frame.t > prevT) fps = fps > 0 ? 0.9 * fps + 0.1 / (frame.t - prevT) : 1 / (frame.t - prevT);
        prevT = frame.t;

        const ControllerStatus& st = ctl.status();
        if (now - lastStatus > 0.25) {
            lastStatus = now;
            std::string eyes;
            for (int k = 0; k < 2; ++k) {
                const EyeMeasurement& m = fe.eye[k];
                eyes += fmt("%s{\"tracked\":%s,\"valid\":%s,\"x\":%.2f,\"y\":%.2f,\"r\":%.2f,\"conf\":%.2f,\"glint\":%s,\"gx\":%.2f,\"gy\":%.2f}",
                            k ? "," : "", fe.tracked[k] ? "true" : "false", m.valid ? "true" : "false", m.pupil.x, m.pupil.y,
                            m.radius, m.confidence, m.hasGlint ? "true" : "false", m.glint.x, m.glint.y);
            }
            std::string js = "{";
            js += fmt("\"mode\":\"%s\",\"fps\":%.1f,\"calibrated\":%s,\"rms\":[%.4f,%.4f],", modeName(st.mode), fps,
                      st.calibrated ? "true" : "false", st.rms[0], st.rms[1]);
            js += fmt("\"calib\":{\"point\":%d,\"points\":%d,\"report\":\"", st.calibPoint, st.calibPoints) +
                  jsonEscape(st.lastCalibReport) + "\"},";
            js += fmt("\"gaze\":{\"valid\":%s,\"x\":%.4f,\"y\":%.4f},\"ipd\":%.1f,", st.gazeValid ? "true" : "false",
                      st.gaze.x, st.gaze.y, fe.ipdPx);
            js += "\"eyes\":[" + eyes + "],";
            js += fmt("\"hid\":{\"enabled\":%s,\"open\":%s,\"usb\":%s,\"sent\":%ld,\"dropped\":%ld},", hidWanted ? "true" : "false",
                      serial.isOpen() ? "true" : "false", hidUsb ? "true" : "false", hid.sent(), hid.dropped());
            js += fmt("\"camera\":{\"running\":%s,\"frames\":%ld,\"width\":%d,\"height\":%d}}", cam.running() ? "true" : "false",
                      cam.framesRead(), frame.width, frame.height);
            std::lock_guard<std::mutex> lk(shared.m);
            shared.status = js;
        }
        if (now - lastSnap > 0.15) {                     // debug picture only while someone watches
            std::lock_guard<std::mutex> lk(shared.m);
            if (now - shared.lastFrameRequest < 2.0 || !shared.haveFrame) {
                lastSnap = now;
                shared.frame.y = frame.y;
                shared.frame.width = frame.width;
                shared.frame.height = frame.height;
                shared.eyes = fe;
                shared.haveFrame = true;
            }
        }
        if (now - lastLog >= cfg.logIntervalS) {
            logLine("fps %.1f | eyes: any %ld%% both %ld%% | ipd %.0f px | %s | gaze %s | hid sent %ld dropped %ld",
                 fps, nFrames ? 100 * nAny / nFrames : 0, nFrames ? 100 * nBoth / nFrames : 0, fe.ipdPx, modeName(st.mode),
                 st.gazeValid ? fmt("%.2f,%.2f", st.gaze.x, st.gaze.y).c_str() : "-", hid.sent(), hid.dropped());
            lastLog = now;
            nFrames = nBoth = nAny = 0;
        }
    }
    logLine("stopping");
    cam.stop();
    http.stop();
    fifo.stop();
    button.stop();
    return 0;
}
