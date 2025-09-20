#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>

static const char* kSockPath = "/tmp/demo.sock";

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

    ::unlink(kSockPath);
    ::close(client_fd);
    ::close(srv);
    return 0;
}