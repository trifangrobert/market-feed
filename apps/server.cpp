#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstdint>
#include "wire.hpp"
#include "codec.hpp"

static const char* kSockPath = "/tmp/demo.sock";

static bool read_exact(int fd, void *buf, size_t n) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t done = 0;
    while (done < n) {
        ssize_t read_bytes = ::read(fd, p + done, n - done);
        if (read_bytes == 0) {
            return false; // peer closed
        }
        if (read_bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        // casting from signed to unsigned
        done += size_t(read_bytes);
    }
    return true;
}

static bool write_all(int fd, const void *buf, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    while (n > 0) {
        ssize_t written_bytes = ::write(fd, p, n);
        if (written_bytes == 0) {
            return false; // peer closed
        }
        if (written_bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += size_t(written_bytes);
        n -= size_t(written_bytes);
    }
    return true;
}

int main() {
    int srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) {
        std::perror("socket");
        return 1;
    }
    ::unlink(kSockPath);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kSockPath, sizeof(addr.sun_path) - 1);

    if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(srv);
        return 1;
    }

    if (::listen(srv, 1) < 0) {
        std::perror("listen");
        ::close(srv);
        return 1;
    }

    std::cout << "server: listening on " << kSockPath << "\n";

    int client_fd;
    for (;;) {
        client_fd = ::accept(srv, nullptr, nullptr);
        if (client_fd >= 0) break;
        if (errno == EINTR) continue;
        std::perror("accept"); ::close(srv); return 1;
    }
    
    std::cout << "server: client connected\n";
    
    constexpr size_t kMaxFrame = 64 * 1024;
    Header h{};
    if (!read_exact(client_fd, &h, sizeof(Header))) {
        std::cerr << "server: failed to read header\n";
    }
    else {
        std::cout << "got header type=" << int(h.type)
              << " ver=" << int(h.version)
              << " size=" << h.size << "\n";
        if (h.size < sizeof(Header)) {
            std::cerr << "server: bad frame size\n";
        }
        else {
            const size_t body_len = h.size - sizeof(Header);
            if (body_len > kMaxFrame) {
                std::cerr << "server: frame too large\n";
                throw std::runtime_error("frame too large");
            }
            std::vector<uint8_t> body(body_len);
            if (!read_exact(client_fd, body.data(), body_len)) {
                std::cerr << "server: failed to read body\n";
            }
            else {
                switch (static_cast<MsgType>(h.type)) {
                    case MsgType::NEW:
                        std::cout << "got header type=NEW" << "\n";
                        break;
                    case MsgType::CANCEL:
                        std::cout << "got header type=CANCEL" << "\n";
                        break;
                    case MsgType::ACK:
                        std::cout << "got header type=ACK" << "\n";
                        break;
                    case MsgType::TRADE:
                        std::cout << "got header type=TRADE" << "\n";
                        break;
                    case MsgType::RESERVED:
                        std::cout << "got header type=RESERVED" << "\n" << "\n";
                        break;
                    default:
                        std::cout << "got header type=UNKNOWN(" << int(h.type) << ")" << "\n";
                        break;
                }
            }
        }
    }

    ::unlink(kSockPath);
    ::close(client_fd);
    ::close(srv);
    return 0;
}