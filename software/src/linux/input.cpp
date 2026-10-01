// Pi 5 power button and the control FIFO.
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "pigaze/platform.hpp"

namespace pigaze {

bool PowerButton::start(std::function<void(ButtonEvent)> cb, std::string& err) {
    stop();
    for (int i = 0; i < 32 && fd_ < 0; ++i) {
        const std::string path = "/dev/input/event" + std::to_string(i);
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        char name[128] = {0};
        if (ioctl(fd, EVIOCGNAME(sizeof name - 1), name) >= 0 && std::strstr(name, "pwr_button")) fd_ = fd;
        else ::close(fd);
    }
    if (fd_ < 0) { err = "no 'pwr_button' input device (not a Pi 5, or no access to /dev/input)"; return false; }
    stop_ = false;
    thread_ = std::thread([this, cb] {
        PressClassifier cls;
        while (!stop_) {
            pollfd p{fd_, POLLIN, 0};
            const int r = poll(&p, 1, 100);
            const double now = monotonicSeconds();
            if (r > 0 && (p.revents & POLLIN)) {
                input_event ev;
                while (::read(fd_, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev)) {
                    if (ev.type != EV_KEY || ev.code != KEY_POWER) continue;
                    if (ev.value == 1) cls.press(now);
                    else if (ev.value == 0) {
                        const ButtonEvent e = cls.release(now);
                        if (e != ButtonEvent::None) cb(e);
                    }
                }
            }
            const ButtonEvent e = cls.tick(now);
            if (e != ButtonEvent::None) cb(e);
        }
    });
    return true;
}

void PowerButton::stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool ControlFifo::start(const std::string& path, std::function<void(const std::string&)> cb, std::string& err) {
    stop();
    struct stat st {};
    if (::stat(path.c_str(), &st) == 0) {
        if (!S_ISFIFO(st.st_mode)) { err = path + " exists and is not a FIFO"; return false; }
    } else if (mkfifo(path.c_str(), 0660) != 0) {
        err = path + ": " + std::strerror(errno);
        return false;
    }
    fd_ = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);   // RDWR: no EOF when writers leave
    if (fd_ < 0) { err = path + ": " + std::strerror(errno); return false; }
    stop_ = false;
    thread_ = std::thread([this, cb] {
        std::string buf;
        char chunk[256];
        while (!stop_) {
            pollfd p{fd_, POLLIN, 0};
            if (poll(&p, 1, 200) <= 0) continue;
            ssize_t r;
            while ((r = ::read(fd_, chunk, sizeof chunk)) > 0) buf.append(chunk, static_cast<size_t>(r));
            for (size_t nl; (nl = buf.find('\n')) != std::string::npos;) {
                std::string line = buf.substr(0, nl);
                buf.erase(0, nl + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (!line.empty()) cb(line);
            }
            if (buf.size() > 4096) buf.clear();
        }
    });
    return true;
}

void ControlFifo::stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

}  // namespace pigaze
