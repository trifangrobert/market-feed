#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

#include "network/connection.hpp"
#include "engine.hpp"
#include "handlers/server_message_handler.hpp"

class TradingServer {
public:
    // Unix socket constructor
    explicit TradingServer(const std::string& socket_path);
    
    // internet socket constructor  
    TradingServer(const std::string& host, uint16_t port, int socket_type = SOCK_STREAM);
    
    ~TradingServer();
    
    void run();
    void stop();
    
private:
    int address_family_;
    int socket_type_;
    std::variant<sockaddr_un, sockaddr_in> addr_;
    socklen_t addr_len_;
    int server_socket_;
    bool running_;
    std::unordered_map<ClientId, Connection> client_connections_;
    Engine engine_;
    ServerMessageHandler message_handler_;
    
    void setup_unix_address(const std::string& socket_path);
    void setup_inet_address(const std::string& host, uint16_t port);
    void create_and_bind_socket();
    
    void accept_new_clients();
    void handle_existing_clients(const fd_set& read_fds);
    bool process_client_message(Connection& client);
};