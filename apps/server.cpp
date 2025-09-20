#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include "wire.hpp"
#include <cstdint>

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

    int client_fd = ::accept(srv, nullptr, nullptr);
    if (client_fd < 0) {
        std::perror("accept");
        ::close(srv);
        return 1;
    }

    std::cout << "server: client connected\n";

    Header h{};
    if (!read_exact(client_fd, &h, sizeof(Header))) {
        std::cerr << "server: failed to read header\n";
    }
    else {
        std::cout << "got header type=" << int(h.type)
              << " ver=" << int(h.version)
              << " size=" << h.size << "\n";
    }

    ::unlink(kSockPath);
    ::close(client_fd);
    ::close(srv);
    return 0;
}