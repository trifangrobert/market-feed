#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <vector>
#include <span>

#include "wire.hpp"
#include "codec.hpp"
#include "order_book.hpp"

static const char* kSockPath = "/tmp/demo.sock";

static uint64_t now_ns() noexcept {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

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
    int server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::perror("error creating client socket");
        return 1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kSockPath, sizeof(addr.sun_path) - 1);

    if (::connect(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect to server failed");
        ::close(server_fd);
        return 1;
    }

    std::cout << "client: connected to server at " << kSockPath << "\n"; 

    Header hdr{};
    hdr.seqno = 0;
    hdr.type = static_cast<uint8_t>(MsgType::NEW);
    hdr.version = kProtocolVersion;
    hdr.size = sizeof(Header) + sizeof(OrderNewBody);
    hdr.ts_ns = now_ns();

    OrderNewBody body{};
    body.client_order_id = 42;
    body.price_ticks = 101;
    body.qty = 30;
    body.instrument_id = 1;
    body.side = static_cast<uint8_t>(OrderSide::Bid);
    body.flags = 0;

    std::vector<uint8_t> bytes = codec::pack(hdr, body);
    
    if (!write_all(server_fd, bytes.data(), bytes.size())) {
        std::cerr << "client: failed to send order\n";
        ::close(server_fd);
        return 1;
    }

    std::cout << "client: sent NEW order (cid=" << body.client_order_id 
              << ", qty=" << body.qty 
              << ", price=" << body.price_ticks << ")\n";

    Header ack_hdr{};
    if (!read_exact(server_fd, &ack_hdr, sizeof(Header))) {
        std::cerr << "client: failed to receive ACK for NEW order\n";
        ::close(server_fd);
        return 1;
    }

    std::cout << "client: received ACK message type=" << int(ack_hdr.type)
              << " size=" << ack_hdr.size << "\n";

    if (ack_hdr.size < sizeof(Header) || ack_hdr.size - sizeof(Header) > kMaxFrame) {
        std::cerr << "client: bad ACK frame size\n";
        ::close(server_fd);
        return 1;
    }

    const size_t ack_body_len = ack_hdr.size - sizeof(Header);
    std::vector<uint8_t> ack_body_data(ack_body_len);
    if (!read_exact(server_fd, ack_body_data.data(), ack_body_len)) {
        std::cerr << "client: failed to read ACK body\n";
        ::close(server_fd);
        return 1;
    }

    uint64_t exch_order_id = 0;
    if (static_cast<MsgType>(ack_hdr.type) == MsgType::ACK) {
        try {
            auto ack = codec::decode_body<AckBody>(std::span<const uint8_t>(ack_body_data));
            std::cout << "NEW ACK: cid=" << ack.client_order_id
                      << " exch_oid=" << ack.exch_order_id
                      << " status=" << int(ack.status) << (ack.status == 0 ? " (ACCEPTED)" : " (REJECTED)")
                      << " recv_ts=" << ack.ts_engine_recv_ns
                      << " ack_ts=" << ack.ts_engine_ack_ns << "\n";
            
            if (ack.status == 0) { // Only proceed if order was accepted
                exch_order_id = ack.exch_order_id;
            } else {
                std::cout << "client: NEW order was rejected, cannot cancel\n";
                ::close(server_fd);
                return 0;
            }
        } catch (const std::exception& e) {
            std::cerr << "client: failed to decode NEW ACK: " << e.what() << "\n";
            ::close(server_fd);
            return 1;
        }
    } else {
        std::cerr << "client: expected ACK but got message type " << int(ack_hdr.type) << "\n";
        ::close(server_fd);
        return 1;
    }

    std::cout << "\nclient: sending CANCEL order for exch_oid=" << exch_order_id << "\n";
    
    Header cancel_hdr{};
    cancel_hdr.seqno = 1;
    cancel_hdr.type = static_cast<uint8_t>(MsgType::CANCEL);
    cancel_hdr.version = kProtocolVersion;
    cancel_hdr.size = sizeof(Header) + sizeof(OrderCancelBody);
    cancel_hdr.ts_ns = now_ns();

    OrderCancelBody cancel_body{};
    cancel_body.client_order_id = body.client_order_id; // Same client order ID
    cancel_body.exch_order_id = exch_order_id;          // Exchange order ID from ACK
    cancel_body.instrument_id = body.instrument_id;     // Same instrument
    cancel_body.reason_code = 0;                        // User requested cancel

    std::vector<uint8_t> cancel_bytes = codec::pack(cancel_hdr, cancel_body);
    if (!write_all(server_fd, cancel_bytes.data(), cancel_bytes.size())) {
        std::cerr << "client: failed to send CANCEL order\n";
        ::close(server_fd);
        return 1;
    }

    std::cout << "client: sent CANCEL order (cid=" << cancel_body.client_order_id 
              << ", exch_oid=" << cancel_body.exch_order_id << ")\n";

    // Step 3: Receive ACK for the CANCEL order
    Header cancel_ack_hdr{};
    if (!read_exact(server_fd, &cancel_ack_hdr, sizeof(Header))) {
        std::cerr << "client: failed to receive ACK for CANCEL order\n";
        ::close(server_fd);
        return 1;
    }

    std::cout << "client: received CANCEL ACK message type=" << int(cancel_ack_hdr.type)
              << " size=" << cancel_ack_hdr.size << "\n";

    if (cancel_ack_hdr.size < sizeof(Header) || cancel_ack_hdr.size - sizeof(Header) > kMaxFrame) {
        std::cerr << "client: bad CANCEL ACK frame size\n";
        ::close(server_fd);
        return 1;
    }

    const size_t cancel_ack_body_len = cancel_ack_hdr.size - sizeof(Header);
    std::vector<uint8_t> cancel_ack_body_data(cancel_ack_body_len);
    if (!read_exact(server_fd, cancel_ack_body_data.data(), cancel_ack_body_len)) {
        std::cerr << "client: failed to read CANCEL ACK body\n";
        ::close(server_fd);
        return 1;
    }

    if (static_cast<MsgType>(cancel_ack_hdr.type) == MsgType::ACK) {
        try {
            auto cancel_ack = codec::decode_body<AckBody>(std::span<const uint8_t>(cancel_ack_body_data));
            std::cout << "CANCEL ACK: cid=" << cancel_ack.client_order_id
                      << " exch_oid=" << cancel_ack.exch_order_id
                      << " status=" << int(cancel_ack.status) << (cancel_ack.status == 0 ? " (ACCEPTED)" : " (REJECTED)")
                      << " recv_ts=" << cancel_ack.ts_engine_recv_ns
                      << " ack_ts=" << cancel_ack.ts_engine_ack_ns << "\n";
        } catch (const std::exception& e) {
            std::cerr << "client: failed to decode CANCEL ACK: " << e.what() << "\n";
        }
    } else {
        std::cerr << "client: expected ACK for CANCEL but got message type " << int(cancel_ack_hdr.type) << "\n";
    }

    std::cout << "\nclient: workflow completed successfully\n";
    
    ::close(server_fd);
    return 0;
}
