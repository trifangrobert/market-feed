#include "network/connection.hpp"
#include <unistd.h>
#include <errno.h>
#include <cstdint>
#include <cstring>

ClientId Connection::next_client_id_ = 1;

Connection::Connection(int fd) : fd_(fd), connected_(true) {
    client_id_ = next_client_id_++;
}

Connection::~Connection() {
    if (fd_ != -1) {
        ::close(fd_);
    }
    connected_ = false;
}

Connection::Connection(Connection&& other) noexcept 
    : client_id_(other.client_id_), 
      connected_(other.connected_), 
      fd_(other.fd_) {
    
    other.client_id_ = 0;
    other.connected_ = false;
    other.fd_ = -1;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        ::close(fd_);
        this->client_id_ = other.client_id_;
        this->connected_ = other.connected_;
        this->fd_ = other.fd_;

        other.client_id_ = 0;
        other.connected_ = false;
        other.fd_ = -1;
    }
    return *this;
}

bool Connection::send_message(const std::vector<uint8_t>& buf) {
    const uint8_t* p = static_cast<const uint8_t*>(buf.data());
    size_t sz = buf.size();
    while (sz > 0) {
        ssize_t written_bytes = ::write(fd_, p, sz);
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
        sz -= size_t(written_bytes);
    }
    return true;
}

bool Connection::receive_message(std::vector<uint8_t>& buf, size_t expected_byte_length) {
    if (buf.size() < expected_byte_length) {
        buf.resize(expected_byte_length);
    }
    uint8_t* p = buf.data();
    size_t sz = 0;
    while (sz < expected_byte_length) {
        ssize_t read_bytes = ::read(fd_, p + sz, expected_byte_length - sz);
        if (read_bytes == 0) {
            return false;
        }
        if (read_bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        sz += read_bytes;
    }
    return true;
}

bool Connection::is_connected() {
    return connected_;
}

void Connection::close() {
    ::close(fd_);
    connected_ = false;
}
