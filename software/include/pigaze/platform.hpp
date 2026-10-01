// Pi-Gaze — Linux-only I/O: camera pipe, UART, power button, control FIFO, debug HTTP.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "pigaze/button_logic.hpp"
#include "pigaze/ch9329.hpp"
#include "pigaze/config.hpp"

namespace pigaze {

double monotonicSeconds();

struct Frame {
    std::vector<uint8_t> y;      // luma plane, width*height
    int width = 0, height = 0;
    double t = 0;                // monotonic seconds at arrival
    long seq = 0;
    ImageView view() const { return {y.data(), width, height, width}; }
};

// Runs `rpicam-vid ... --codec yuv420 -o -` and hands out the newest frame; frames that
// arrive while the previous one is still being processed are dropped (no latency build-up).
class RpicamSource {
public:
    explicit RpicamSource(const AppConfig& c) : cfg_(c) {}
    ~RpicamSource() { stop(); }
    bool start(std::string& err);
    void stop();
    bool next(Frame& out, int timeoutMs);   // false on timeout or when the camera died
    bool running() const { return alive_; }
    std::vector<std::string> args() const;
    long framesRead() const { return seq_; }

private:
    void readerLoop();
    AppConfig cfg_;
    int pid_ = -1, fd_ = -1;
    std::thread thread_;
    std::atomic<bool> stop_{false}, alive_{false};
    std::mutex m_;
    std::condition_variable cv_;
    Frame latest_;
    bool fresh_ = false;
    std::atomic<long> seq_{0};
};

class SerialLink : public ByteLink {
public:
    ~SerialLink() override { close(); }
    bool open(const std::string& dev, int baud, std::string& err);
    void close();
    bool isOpen() const { return fd_ >= 0; }
    bool write(const uint8_t* d, size_t n) override;
    int read(uint8_t* d, size_t n) override;
    size_t queued() const override;

private:
    int fd_ = -1;
};

// Pi 5 power button (input device "pwr_button"). Long presses are left to logind (power off).
class PowerButton {
public:
    ~PowerButton() { stop(); }
    bool start(std::function<void(ButtonEvent)> cb, std::string& err);
    void stop();

private:
    int fd_ = -1;
    std::thread thread_;
    std::atomic<bool> stop_{false};
};

// One command per line written into a FIFO (see deploy/pigaze-ctl).
class ControlFifo {
public:
    ~ControlFifo() { stop(); }
    bool start(const std::string& path, std::function<void(const std::string&)> cb, std::string& err);
    void stop();

private:
    int fd_ = -1;
    std::thread thread_;
    std::atomic<bool> stop_{false};
};

// Tiny single-threaded HTTP/1.0 server for the debug page.
class DebugHttp {
public:
    // Returns false for 404. `method` is "GET" or "POST".
    using Handler = std::function<bool(const std::string& method, const std::string& path,
                                       std::string& contentType, std::string& body)>;
    ~DebugHttp() { stop(); }
    bool start(const std::string& bind, int port, Handler h, std::string& err);
    void stop();

private:
    int fd_ = -1;
    std::thread thread_;
    std::atomic<bool> stop_{false};
};

}  // namespace pigaze
