// Camera frames from `rpicam-vid --codec yuv420 -o -` (rpicam-apps ships with Raspberry Pi OS).
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>

#include "pigaze/platform.hpp"

extern char** environ;

namespace pigaze {
namespace {

std::string num(double v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

bool readFully(int fd, uint8_t* p, size_t n) {
    while (n > 0) {
        const ssize_t r = ::read(fd, p, n);
        if (r > 0) { p += r; n -= static_cast<size_t>(r); continue; }
        if (r < 0 && errno == EINTR) continue;
        return false;                                     // EOF: rpicam-vid exited
    }
    return true;
}

}  // namespace

double monotonicSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::vector<std::string> RpicamSource::args() const {
    std::vector<std::string> a = {"rpicam-vid", "-t", "0", "-n", "--verbose", "0", "--codec", "yuv420",
                                  "--width", std::to_string(cfg_.width), "--height", std::to_string(cfg_.height),
                                  "--framerate", std::to_string(cfg_.fps), "--denoise", "cdn_off"};
    if (cfg_.shutterUs > 0) { a.push_back("--shutter"); a.push_back(std::to_string(cfg_.shutterUs)); }
    if (cfg_.gain > 0) { a.push_back("--gain"); a.push_back(num(cfg_.gain)); }
    if (cfg_.lensPosition >= 0) {
        for (const char* s : {"--autofocus-mode", "manual", "--lens-position"}) a.push_back(s);
        a.push_back(num(cfg_.lensPosition));
    } else {
        for (const char* s : {"--autofocus-mode", "continuous"}) a.push_back(s);
    }
    std::istringstream extra(cfg_.cameraExtra);
    for (std::string w; extra >> w;) a.push_back(w);
    a.push_back("-o");
    a.push_back("-");
    return a;
}

bool RpicamSource::start(std::string& err) {
    stop();
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) { err = std::string("pipe: ") + std::strerror(errno); return false; }

    // everything the child needs is prepared before fork (only async-signal-safe calls after it)
    const std::vector<std::string> a = args();
    std::vector<char*> argv;
    for (const std::string& s : a) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
    std::string logLevels = "LIBCAMERA_LOG_LEVELS=*:WARN";
    std::vector<char*> envp;
    for (char** e = environ; e && *e; ++e)
        if (std::strncmp(*e, "LIBCAMERA_LOG_LEVELS=", 21) != 0) envp.push_back(*e);
    envp.push_back(&logLevels[0]);
    envp.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        err = std::string("fork: ") + std::strerror(errno);
        ::close(fds[0]);
        ::close(fds[1]);
        return false;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        const int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) dup2(devnull, STDIN_FILENO);
        execvpe(argv[0], argv.data(), envp.data());
        _exit(127);
    }
    ::close(fds[1]);
    fd_ = fds[0];
    pid_ = pid;
#ifdef F_SETPIPE_SZ
    fcntl(fd_, F_SETPIPE_SZ, 1 << 20);                  // fewer wake-ups per 4.5 MB frame
#endif
    stop_ = false;
    alive_ = true;
    fresh_ = false;
    thread_ = std::thread(&RpicamSource::readerLoop, this);
    return true;
}

void RpicamSource::readerLoop() {
    const size_t ySize = static_cast<size_t>(cfg_.width) * cfg_.height, uvSize = ySize / 2;
    std::vector<uint8_t> back(ySize), uv(uvSize);
    while (!stop_) {
        if (back.size() != ySize) back.resize(ySize);
        if (!readFully(fd_, back.data(), ySize) || !readFully(fd_, uv.data(), uvSize)) break;
        const double t = monotonicSeconds();
        {
            std::lock_guard<std::mutex> lk(m_);
            latest_.y.swap(back);                         // newest frame wins; an unread one is dropped
            latest_.width = cfg_.width;
            latest_.height = cfg_.height;
            latest_.t = t;
            latest_.seq = ++seq_;
            fresh_ = true;
        }
        cv_.notify_one();
    }
    alive_ = false;
    cv_.notify_all();
}

bool RpicamSource::next(Frame& out, int timeoutMs) {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this] { return fresh_ || !alive_; });
    if (!fresh_) return false;
    out.y.swap(latest_.y);
    out.width = latest_.width;
    out.height = latest_.height;
    out.t = latest_.t;
    out.seq = latest_.seq;
    fresh_ = false;
    return true;
}

void RpicamSource::stop() {
    stop_ = true;
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        int status = 0;
        bool gone = false;
        for (int i = 0; i < 40 && !gone; ++i) {           // up to 2 s for a clean exit
            gone = waitpid(pid_, &status, WNOHANG) == pid_;
            if (!gone) usleep(50000);
        }
        if (!gone) { kill(pid_, SIGKILL); waitpid(pid_, &status, 0); }
        pid_ = -1;
    }
    if (thread_.joinable()) thread_.join();              // the pipe is at EOF now
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    alive_ = false;
}

}  // namespace pigaze
