#include "trading/trading_server.hpp"
#include "wire.hpp"

#include <string>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <fcntl.h>
#include <sys/select.h>
#include <algorithm>

// UNIX socket constructor
TradingServer::TradingServer(const std::string& socket_path) 
    : address_family_(AF_UNIX), socket_type_(SOCK_STREAM), server_socket_(-1), running_(false), 
      message_handler_(engine_) {
    setup_unix_address(socket_path);
    create_and_bind_socket();
}

// Internet socket constructor  
TradingServer::TradingServer(const std::string& host, uint16_t port, int socket_type)
    : address_family_(AF_INET), socket_type_(socket_type), server_socket_(-1), running_(false),
      message_handler_(engine_) {
    setup_inet_address(host, port);
    create_and_bind_socket();
}

TradingServer::~TradingServer() {
    if (server_socket_ >= 0) {
        ::close(server_socket_);
    }
}

void TradingServer::setup_unix_address(const std::string& socket_path) {
    sockaddr_un unix_addr{};
    unix_addr.sun_family = AF_UNIX;
    std::strncpy(unix_addr.sun_path, socket_path.c_str(), sizeof(unix_addr.sun_path) - 1);
    
    addr_ = unix_addr;
    addr_len_ = sizeof(unix_addr);
    
    ::unlink(socket_path.c_str());
}

void TradingServer::setup_inet_address(const std::string& host, uint16_t port) {
    sockaddr_in inet_addr{};
    inet_addr.sin_family = AF_INET;
    inet_addr.sin_port = htons(port);
    
    if (host == "localhost" || host == "127.0.0.1") {
        inet_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (host == "0.0.0.0") {
        inet_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, host.c_str(), &inet_addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid IP address: " + host);
        }
    }
    
    addr_ = inet_addr;
    addr_len_ = sizeof(inet_addr);
}

void TradingServer::create_and_bind_socket() {
    server_socket_ = ::socket(address_family_, socket_type_, 0);
    if (server_socket_ < 0) {
        throw std::runtime_error("Failed to create server socket: " + std::string(strerror(errno)));
    }
    
    if (address_family_ == AF_INET) {
        int opt = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            ::close(server_socket_);
            throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        }
    }
    
    const sockaddr* addr_ptr = nullptr;
    if (address_family_ == AF_UNIX) {
        addr_ptr = reinterpret_cast<const sockaddr*>(&std::get<sockaddr_un>(addr_));
    } else {
        addr_ptr = reinterpret_cast<const sockaddr*>(&std::get<sockaddr_in>(addr_));
    }
    
    if (::bind(server_socket_, addr_ptr, addr_len_) < 0) {
        ::close(server_socket_);
        throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
    }
    
    if (::listen(server_socket_, 5) < 0) {
        ::close(server_socket_);
        throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
    }
}

void TradingServer::run() {
    running_ = true;
    std::cout << "TradingServer: Starting main loop\n";
    
    while (running_) {
        // fd_set is like a bitset 
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        FD_SET(server_socket_, &read_fds);
        int max_fd = server_socket_;
        
        for (auto& [client_id, connection] : client_connections_) {
            int client_fd = connection.get_fd();
            FD_SET(client_fd, &read_fds);
            max_fd = std::max(max_fd, client_fd);
        }
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (activity < 0) {
            if (errno == EINTR) continue; // Interrupted by signal, continue
            std::cerr << "TradingServer: select() failed: " << strerror(errno) << "\n";
            break;
        }
        
        if (activity == 0) {
            continue;
        }
        
        // only server_socket_ fd can accept new clients
        if (FD_ISSET(server_socket_, &read_fds)) {
            accept_new_clients();
        }
        
        // check all clients for new data
        handle_existing_clients(read_fds);
    }
    
    std::cout << "TradingServer: Main loop stopped\n";
}

void TradingServer::stop() {
    running_ = false;
}

void TradingServer::accept_new_clients() {
    int client_fd = ::accept(server_socket_, nullptr, nullptr);
    
    if (client_fd >= 0) { // new client connected here
        Connection new_client(client_fd);
        ClientId client_id = new_client.get_client_id();
        
        std::cout << "TradingServer: New client connected, ID: " << client_id << "\n";
        
        client_connections_.emplace(client_id, std::move(new_client));
    } else {
        std::cerr << "TradingServer: Accept failed: " << strerror(errno) << "\n";
    }
}

void TradingServer::handle_existing_clients(const fd_set& read_fds) {
    auto it = client_connections_.begin();
    while (it != client_connections_.end()) {
        Connection& client = it->second;
        
        if (!client.is_connected()) {
            std::cout << "TradingServer: Client " << it->first << " disconnected\n";
            it = client_connections_.erase(it);
            continue;
        }
        
        if (FD_ISSET(client.get_fd(), &read_fds)) {
            if (!process_client_message(client)) {
                std::cout << "TradingServer: Client " << it->first << " disconnected gracefully\n";
                it = client_connections_.erase(it);
                continue;
            }
        }
        
        ++it;
    }
}

bool TradingServer::process_client_message(Connection& client) {
    std::vector<uint8_t> message;

    message.resize(sizeof(Header));

    if (!client.receive_message(message, sizeof(Header))) {
        return false;
    }

    Header* header = reinterpret_cast<Header*>(message.data());
    if (header->size < sizeof(Header) || header->size > kMaxFrame) {
        std::cerr << "TradingServer: Invalid message size " << header->size 
                  << " from client " << client.get_client_id() << "\n";
        return false;
    }

    size_t body_size = header->size - sizeof(Header);
    if (body_size > 0) {
        message.resize(header->size);
        std::vector<uint8_t> body_bytes;
        if (!client.receive_message(body_bytes, body_size)) {
            std::cerr << "TradingServer: Failed to read message body from client " 
                      << client.get_client_id() << "\n";
            return false;
        }

        std::memcpy(message.data() + sizeof(Header), body_bytes.data(), body_size);
    }
    auto responses = message_handler_.handle_message(message);

    for (const auto& response : responses) {
        if (!client.send_message(response)) {
            std::cerr << "TradingServer: Failed to send response to client " 
                      << client.get_client_id() << "\n";
            return false;
        }
    }
    
    return true;
}