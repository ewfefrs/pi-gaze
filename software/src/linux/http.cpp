// Minimal HTTP/1.0 server for the debug page (one connection at a time).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

#include "pigaze/platform.hpp"

namespace pigaze {
namespace {

void sendAll(int fd, const std::string& s) {
    size_t off = 0;
    while (off < s.size()) {
        const ssize_t r = send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (r <= 0) return;
        off += static_cast<size_t>(r);
    }
}

}  // namespace

bool DebugHttp::start(const std::string& bind, int port, Handler h, std::string& err) {
    stop();
    const int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { err = std::strerror(errno); return false; }
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, bind.c_str(), &a.sin_addr) != 1) { err = "bad http_bind address " + bind; ::close(fd); return false; }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(fd, 8) != 0) {
        err = bind + ":" + std::to_string(port) + ": " + std::strerror(errno);
        ::close(fd);
        return false;
    }
    fd_ = fd;
    stop_ = false;
    thread_ = std::thread([this, h] {
        while (!stop_) {
            pollfd p{fd_, POLLIN, 0};
            if (poll(&p, 1, 200) <= 0) continue;
            const int c = accept4(fd_, nullptr, nullptr, SOCK_CLOEXEC);
            if (c < 0) continue;
            timeval tv{3, 0};
            setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
            setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
            std::string req;
            char buf[2048];
            while (req.find("\r\n\r\n") == std::string::npos && req.size() < 8192) {
                const ssize_t r = recv(c, buf, sizeof buf, 0);
                if (r <= 0) break;
                req.append(buf, static_cast<size_t>(r));
            }
            std::istringstream line(req);
            std::string method, path;
            line >> method >> path;
            const size_t q = path.find('?');
            if (q != std::string::npos) path.erase(q);
            std::string type = "text/plain; charset=utf-8", body;
            const bool found = (method == "GET" || method == "POST") && h(method, path, type, body);
            if (!found) { type = "text/plain; charset=utf-8"; body = "not found"; }
            std::ostringstream head;
            head << "HTTP/1.0 " << (found ? "200 OK" : "404 Not Found") << "\r\nContent-Type: " << type
                 << "\r\nContent-Length: " << body.size() << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
            sendAll(c, head.str());
            sendAll(c, body);
            ::close(c);
        }
    });
    return true;
}

void DebugHttp::stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

}  // namespace pigaze
