#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstdint>
#include <chrono>
#include <vector>
#include <span>

#include "wire.hpp"
#include "codec.hpp"
#include "engine.hpp"

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
    
    Engine engine;
    static uint64_t md_seqno = 0;
    
    while (true) {
        Header h{};
        if (!read_exact(client_fd, &h, sizeof(Header))) {
            std::cout << "server: client disconnected\n";
            break;
        }
        
        std::cout << "got header type=" << int(h.type)
                  << " ver=" << int(h.version)
                  << " size=" << h.size << "\n";
        
        if (h.size < sizeof(Header) || h.size - sizeof(Header) > kMaxFrame) {
            std::cerr << "server: bad frame size\n";
            break;
        }
        
        const size_t body_len = h.size - sizeof(Header);
        std::vector<uint8_t> body(body_len);
        if (!read_exact(client_fd, body.data(), body_len)) {
            std::cerr << "server: failed to read body\n";
            break;
        }
        
        switch (static_cast<MsgType>(h.type)) {
            case MsgType::NEW:
                try {
                    auto m = codec::decode_body<OrderNewBody>(std::span<const uint8_t>(body));
                    std::cout << "NEW: cid=" << m.client_order_id
                            << " side=" << int(m.side)
                            << " qty=" << m.qty
                            << " px=" << m.price_ticks
                            << " instr=" << m.instrument_id
                            << " flags=0x" << std::hex << int(m.flags) << std::dec << "\n";
                    bool rest_leftover = ((m.flags & TIF_IOC) == 0); // TODO: properly manage flags
                    EngineResult res = engine.on_new(m, rest_leftover);
                    Header ack_hdr = codec::make_header(MsgType::ACK, sizeof(AckBody), 0, now_ns());
                    auto ack_bytes = codec::pack(ack_hdr, res.ack);
                    if (!write_all(client_fd, ack_bytes.data(), ack_bytes.size())) {
                        std::cerr << "server: failed to send ACK\n";
                        goto client_loop_exit;
                    }
                    
                    for (const auto& trade : res.trades) {
                        Header trade_hdr = codec::make_header(MsgType::TRADE, sizeof(TradeBody), ++md_seqno, now_ns());
                        auto trade_bytes = codec::pack(trade_hdr, trade);
                        if (!write_all(client_fd, trade_bytes.data(), trade_bytes.size())) {
                            std::cerr << "server: failed to send TRADE\n";
                            goto client_loop_exit;
                        }
                    }

                } catch (const std::exception& e) {
                    std::cerr << "decode NEW failed: " << e.what() << "\n";
                }
                break;
            case MsgType::CANCEL:
                try {
                    auto m = codec::decode_body<OrderCancelBody>(std::span<const uint8_t>(body));
                    std::cout << "CANCEL: cid=" << m.client_order_id << "\n";
                            
                    EngineResult res = engine.on_cancel(m);
                    Header ack_hdr = codec::make_header(MsgType::ACK, sizeof(AckBody), 0, now_ns());
                    auto ack_bytes = codec::pack(ack_hdr, res.ack);
                    if (!write_all(client_fd, ack_bytes.data(), ack_bytes.size())) {
                        std::cerr << "server: failed to send ACK\n";
                        goto client_loop_exit;
                    }

                } catch (const std::exception& e) {
                    std::cerr << "decode CANCEL failed: " << e.what() << "\n";
                }
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
    
client_loop_exit:

    ::unlink(kSockPath);
    ::close(client_fd);
    ::close(srv);
    return 0;
}