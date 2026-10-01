#include "pigaze/config.hpp"

#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace pigaze {
namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool toInt(const std::string& v, int& out) {
    try {
        size_t p = 0;
        const long x = std::stol(v, &p);
        if (p != v.size()) return false;
        out = static_cast<int>(x);
        return true;
    } catch (...) { return false; }
}

bool toDouble(const std::string& v, double& out) {
    try {
        size_t p = 0;
        const double x = std::stod(v, &p);
        if (p != v.size()) return false;
        out = x;
        return true;
    } catch (...) { return false; }
}

bool toBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "yes" || v == "on") { out = true; return true; }
    if (v == "0" || v == "false" || v == "no" || v == "off") { out = false; return true; }
    return false;
}

using Setter = std::function<bool(const std::string&)>;
Setter I(int& r) { return [&r](const std::string& v) { return toInt(v, r); }; }
Setter D(double& r) { return [&r](const std::string& v) { return toDouble(v, r); }; }
Setter B(bool& r) { return [&r](const std::string& v) { return toBool(v, r); }; }
Setter S(std::string& r) { return [&r](const std::string& v) { r = v; return true; }; }

}  // namespace

void parseConfig(const std::string& text, AppConfig& c, std::vector<std::string>& w) {
    DetectorParams& d = c.tracker.det;
    ControllerParams& k = c.ctl;
    const std::map<std::string, Setter> keys = {
        {"width", I(c.width)}, {"height", I(c.height)}, {"fps", I(c.fps)},
        {"shutter_us", I(c.shutterUs)}, {"gain", D(c.gain)}, {"lens_position", D(c.lensPosition)},
        {"camera_extra", S(c.cameraExtra)},
        {"eye_roi", I(d.roiSize)}, {"pupil_min_r", D(d.pupilMinR)}, {"pupil_max_r", D(d.pupilMaxR)},
        {"glint_min", I(d.glintMin)}, {"glint_delta", I(d.glintDelta)}, {"glint_max_area", I(d.glintMaxArea)},
        {"min_confidence", D(d.minConfidence)},
        {"ipd_min_frac", D(c.tracker.ipdMinFrac)}, {"ipd_max_frac", D(c.tracker.ipdMaxFrac)},
        {"lost_frames", I(c.tracker.lostFrames)}, {"search_every", I(c.tracker.searchEvery)},
        {"calib_file", S(k.calibFile)}, {"auto_calibrate", B(k.autoCalibrate)},
        {"calib_margin", D(k.calib.margin)}, {"calib_settle_s", D(k.calib.settleS)},
        {"calib_collect_s", D(k.calib.collectS)}, {"calib_max_rms", D(k.calib.maxRms)},
        {"filter_min_cutoff", D(k.minCutoff)}, {"filter_beta", D(k.beta)},
        {"dwell_click", B(k.dwellClick)}, {"dwell_time_s", D(k.dwellS)}, {"dwell_radius", D(k.dwellRadius)},
        {"hid_enabled", B(c.hidEnabled)}, {"hid_device", S(c.hidDevice)}, {"hid_baud", I(c.hidBaud)},
        {"http_port", I(c.httpPort)}, {"http_bind", S(c.httpBind)}, {"log_interval_s", D(c.logIntervalS)},
    };
    std::istringstream in(text);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) { w.push_back("line " + std::to_string(n) + ": expected key = value"); continue; }
        const std::string key = trim(line.substr(0, eq)), val = trim(line.substr(eq + 1));
        const auto it = keys.find(key);
        if (it == keys.end()) { w.push_back("line " + std::to_string(n) + ": unknown key '" + key + "'"); continue; }
        if (!it->second(val)) w.push_back("line " + std::to_string(n) + ": bad value for '" + key + "': " + val);
    }
    if (c.width <= 0 || c.height <= 0 || c.fps <= 0) {
        w.push_back("width/height/fps must be positive: back to 2304x1296@30");
        c.width = 2304; c.height = 1296; c.fps = 30;
    }
    if (c.width % 64 != 0) w.push_back("width should be a multiple of 64 (rpicam pads rows otherwise)");
    if (c.tracker.searchEvery < 1) c.tracker.searchEvery = 1;
}

bool loadConfigFile(const std::string& path, AppConfig& cfg, std::vector<std::string>& warnings) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    parseConfig(ss.str(), cfg, warnings);
    return true;
}

}  // namespace pigaze
