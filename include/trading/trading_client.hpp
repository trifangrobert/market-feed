#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <memory>
#include <string>
#include <variant>

#include "network/connection.hpp"
#include "handlers/client_message_handler.hpp"
#include "order_book.hpp"

using ClientId = uint64_t;

class TradingClient {
public:
    // unix socket
    explicit TradingClient(const std::string& socket_path);
    // internet socket
    TradingClient(const std::string& host, uint16_t port, int socket_type = SOCK_STREAM);
    ~TradingClient();
    static ClientId next_client_id;

    uint64_t place_order(uint32_t instrument_id, OrderSide side, int64_t price_ticks, int32_t qty);
    bool cancel_order(uint64_t exchange_order_id, uint32_t instrument_id);
private:
    void setup_unix_address(const std::string& socket_path);
    void setup_inet_address(const std::string& host, uint16_t port);
    void create_and_connect_socket();

    int address_family_;
    int socket_type_;
    std::variant<sockaddr_un, sockaddr_in> addr_;
    socklen_t addr_len_;
    int client_socket_;
    std::unique_ptr<Connection> connection_;
    ClientMessageHandler message_handler_;
    ClientId client_id_;
    uint64_t next_order_id_;
};