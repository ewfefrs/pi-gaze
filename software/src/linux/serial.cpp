// UART to the CH9329 (raw 8N1, non-blocking).
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "pigaze/platform.hpp"

namespace pigaze {

bool SerialLink::open(const std::string& dev, int baud, std::string& err) {
    close();
    speed_t sp;
    switch (baud) {
        case 9600: sp = B9600; break;
        case 19200: sp = B19200; break;
        case 38400: sp = B38400; break;
        case 57600: sp = B57600; break;
        case 115200: sp = B115200; break;
        default: err = "unsupported baud rate " + std::to_string(baud); return false;
    }
    const int fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { err = dev + ": " + std::strerror(errno); return false; }
    termios tio{};
    if (tcgetattr(fd, &tio) != 0) { err = dev + ": not a tty"; ::close(fd); return false; }
    cfmakeraw(&tio);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(CSTOPB | CRTSCTS | PARENB);
    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) { err = dev + ": " + std::strerror(errno); ::close(fd); return false; }
    tcflush(fd, TCIOFLUSH);
    fd_ = fd;
    return true;
}

void SerialLink::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool SerialLink::write(const uint8_t* d, size_t n) {
    if (fd_ < 0) return false;
    size_t off = 0;
    for (int spins = 0; off < n;) {                      // a frame is never left half-written
        const ssize_t r = ::write(fd_, d + off, n - off);
        if (r > 0) { off += static_cast<size_t>(r); continue; }
        if (r < 0 && (errno == EAGAIN || errno == EINTR) && spins++ < 20) {
            pollfd p{fd_, POLLOUT, 0};
            poll(&p, 1, 10);
            continue;
        }
        return false;
    }
    return true;
}

int SerialLink::read(uint8_t* d, size_t n) {
    if (fd_ < 0) return 0;
    const ssize_t r = ::read(fd_, d, n);
    return r > 0 ? static_cast<int>(r) : 0;
}

size_t SerialLink::queued() const {
    int q = 0;
    if (fd_ >= 0 && ioctl(fd_, TIOCOUTQ, &q) == 0 && q > 0) return static_cast<size_t>(q);
    return 0;
}

}  // namespace pigaze
