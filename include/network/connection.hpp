#pragma once
#include <vector>
#include <cstdint>

using ClientId = uint64_t;

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    // no copy allowed
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // move allowed, but when use it?
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    bool send_message(const std::vector<uint8_t>&);
    bool receive_message(std::vector<uint8_t>&, size_t);
    bool is_connected();
    void close();

    ClientId get_client_id() const {
        return client_id_;
    }
    
    int get_fd() const {
        return fd_;
    }
private:
    int fd_;
    ClientId client_id_;
    bool connected_;

    static ClientId next_client_id_; // will eventually deal with authentication
};