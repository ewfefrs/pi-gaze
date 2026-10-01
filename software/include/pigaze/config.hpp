// Pi-Gaze — configuration (key = value file, '#' comments, defaults for everything).
#pragma once
#include <string>
#include <vector>

#include "pigaze/controller.hpp"
#include "pigaze/tracker.hpp"

namespace pigaze {

struct AppConfig {
    // camera (rpicam-vid)
    int width = 2304, height = 1296, fps = 30;
    int shutterUs = 0;           // 0 = auto exposure
    double gain = 0;             // 0 = auto
    double lensPosition = 1.7;   // dioptres = 1 / distance in metres; < 0 = continuous autofocus
    std::string cameraExtra;     // appended to the rpicam-vid command line
    TrackerParams tracker;
    ControllerParams ctl;
    // CH9329
    bool hidEnabled = true;
    std::string hidDevice = "/dev/ttyAMA0";
    int hidBaud = 9600;
    // debug
    int httpPort = 8080;         // 0 = off
    std::string httpBind = "127.0.0.1";
    double logIntervalS = 5;

    AppConfig() { ctl.calibFile = "/var/lib/pigaze/calibration.txt"; }
};

// Applies "key = value" lines on top of `cfg`. Unknown keys / bad values -> warnings.
void parseConfig(const std::string& text, AppConfig& cfg, std::vector<std::string>& warnings);
bool loadConfigFile(const std::string& path, AppConfig& cfg, std::vector<std::string>& warnings);

}  // namespace pigaze
