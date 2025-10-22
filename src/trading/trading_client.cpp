#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cerrno>

#include "trading/trading_client.hpp"
#include "wire.hpp"

ClientId TradingClient::next_client_id = 1;

// unix socket
TradingClient::TradingClient(const std::string& socket_path) 
    : address_family_(AF_UNIX), socket_type_(SOCK_STREAM), client_socket_(-1), next_order_id_(1) {
    client_id_ = next_client_id++;
    setup_unix_address(socket_path);
    create_and_connect_socket();
}

// internet socket
TradingClient::TradingClient(const std::string& host, uint16_t port, int socket_type) 
    : address_family_(AF_INET), socket_type_(socket_type), client_socket_(-1), next_order_id_(1) {
    client_id_ = next_client_id++;
    setup_inet_address(host, port);
    create_and_connect_socket();
} 

TradingClient::~TradingClient() {

}

void TradingClient::setup_unix_address(const std::string& socket_path) {
    sockaddr_un unix_addr{};
    unix_addr.sun_family = AF_UNIX;
    std::strncpy(unix_addr.sun_path, socket_path.c_str(), sizeof(unix_addr.sun_path) - 1);

    addr_ = unix_addr;
    addr_len_ = sizeof(unix_addr);
}

void TradingClient::setup_inet_address(const std::string& host, uint16_t port) {
    sockaddr_in inet_addr{};
    inet_addr.sin_family = AF_INET;
    inet_addr.sin_port = htons(port);
    
    if (host == "localhost" || host == "127.0.0.1") {
        inet_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (host == "0.0.0.0") {
        inet_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, host.c_str(), &inet_addr.sin_addr) <= 0) {
            throw std::runtime_error("TradingClient: Invalid IP address: " + host);
        }
    }
    
    addr_ = inet_addr;
    addr_len_ = sizeof(inet_addr);
}

void TradingClient::create_and_connect_socket() {
    client_socket_ = ::socket(address_family_, socket_type_, 0);
    if (client_socket_ < 0) {
        throw std::runtime_error("TradingClient: Failed to create socket: " + std::string(strerror(errno)));
    }
    
    const sockaddr* addr_ptr = nullptr;
    if (address_family_ == AF_UNIX) {
        addr_ptr = reinterpret_cast<const sockaddr*>(&std::get<sockaddr_un>(addr_));
    } else {
        addr_ptr = reinterpret_cast<const sockaddr*>(&std::get<sockaddr_in>(addr_));
    }
    
    if (::connect(client_socket_, addr_ptr, addr_len_) < 0) {
        ::close(client_socket_);
        throw std::runtime_error("TradingClient: Failed to connect: " + std::string(strerror(errno)));
    }
    
    connection_ = std::make_unique<Connection>(client_socket_);
}

uint64_t TradingClient::place_order(uint32_t instrument_id, OrderSide side, int64_t price_ticks, int32_t qty) {
    // concatenate the first 32 bits with order id
    uint64_t client_order_id = (static_cast<uint64_t>(client_id_ & 0xFFFFFFFF) << 32) | next_order_id_++;
    
    OrderNewBody new_body{};
    new_body.client_order_id = client_order_id;
    new_body.price_ticks = price_ticks;
    new_body.qty = qty;
    new_body.instrument_id = instrument_id;
    new_body.side = static_cast<uint8_t>(side);
    new_body.flags = 0;
    
    std::vector<uint8_t> bytes_to_send = message_handler_.create_new_order(new_body);
    if (!connection_->send_message(bytes_to_send)) {
        throw std::runtime_error("TradingClient: Failed to send NEW order message");
    }
    
    std::vector<uint8_t> ack_message;
    ack_message.resize(sizeof(Header));
    
    if (!connection_->receive_message(ack_message, sizeof(Header))) {
        throw std::runtime_error("TradingClient: Failed to receive ACK header");
    }
    
    Header* header = reinterpret_cast<Header*>(ack_message.data());
    if (header->type != static_cast<uint8_t>(MsgType::ACK)) {
        throw std::runtime_error("TradingClient: Expected ACK but got message type " + std::to_string(header->type));
    }
    
    size_t body_size = header->size - sizeof(Header);
    if (body_size != sizeof(AckBody)) {
        throw std::runtime_error("TradingClient: Invalid ACK body size");
    }
    
    ack_message.resize(header->size);
    std::vector<uint8_t> ack_body_bytes(body_size);
    if (!connection_->receive_message(ack_body_bytes, body_size)) {
        throw std::runtime_error("TradingClient: Failed to receive ACK body");
    }
    
    std::memcpy(ack_message.data() + sizeof(Header), ack_body_bytes.data(), body_size);
    
    message_handler_.handle_message(ack_message);
    
    const AckBody* ack_body = reinterpret_cast<const AckBody*>(ack_body_bytes.data());
    
    if (ack_body->status != 0) {
        throw std::runtime_error("TradingClient: Order was rejected (NACK) with status " + std::to_string(ack_body->status));
    }
    
    std::vector<uint8_t> additional_message;
    additional_message.resize(sizeof(Header));
    
    if (connection_->receive_message(additional_message, sizeof(Header))) {
        Header* add_header = reinterpret_cast<Header*>(additional_message.data());
        
        size_t add_body_size = add_header->size - sizeof(Header);
        additional_message.resize(add_header->size);
        std::vector<uint8_t> add_body_bytes(add_body_size);
        
        if (connection_->receive_message(add_body_bytes, add_body_size)) {
            std::memcpy(additional_message.data() + sizeof(Header), add_body_bytes.data(), add_body_size);
            message_handler_.handle_message(additional_message);
        }
    }
    
    return ack_body->exch_order_id;
}

bool TradingClient::cancel_order(uint64_t exchange_order_id, uint32_t instrument_id) {
    OrderCancelBody cancel_body{};
    cancel_body.exch_order_id = exchange_order_id;
    cancel_body.client_order_id = 0;
    cancel_body.instrument_id = instrument_id;
    cancel_body.reason_code = 0;
    
    std::vector<uint8_t> bytes_to_send = message_handler_.create_cancel_order(cancel_body);
    if (!connection_->send_message(bytes_to_send)) {
        throw std::runtime_error("TradingClient: Failed to send CANCEL order message");
    }
    
    std::vector<uint8_t> ack_message;
    ack_message.resize(sizeof(Header));
    
    if (!connection_->receive_message(ack_message, sizeof(Header))) {
        throw std::runtime_error("TradingClient: Failed to receive CANCEL ACK header");
    }
    
    Header* header = reinterpret_cast<Header*>(ack_message.data());
    if (header->type != static_cast<uint8_t>(MsgType::ACK)) {
        throw std::runtime_error("TradingClient: Expected ACK but got message type " + std::to_string(header->type));
    }
    
    size_t body_size = header->size - sizeof(Header);
    if (body_size != sizeof(AckBody)) {
        throw std::runtime_error("TradingClient: Invalid CANCEL ACK body size");
    }
    
    ack_message.resize(header->size);
    std::vector<uint8_t> ack_body_bytes(body_size);
    if (!connection_->receive_message(ack_body_bytes, body_size)) {
        throw std::runtime_error("TradingClient: Failed to receive CANCEL ACK body");
    }
    
    std::memcpy(ack_message.data() + sizeof(Header), ack_body_bytes.data(), body_size);
    
    message_handler_.handle_message(ack_message);
    
    const AckBody* ack_body = reinterpret_cast<const AckBody*>(ack_body_bytes.data());

    return ack_body->status == 0;
}

