#pragma once

#include <vector>
#include <chrono>

#include "wire.hpp"
#include "codec.hpp"

class ClientMessageHandler {
public:
    ClientMessageHandler() = default;
    
    void handle_message(const std::vector<uint8_t>&);
    std::vector<uint8_t> create_new_order(const OrderNewBody&);
    std::vector<uint8_t> create_cancel_order(const OrderCancelBody&);
private:
    void handle_ack(const Header& header, const std::vector<uint8_t>& body);
    void handle_trade(const Header& header, const std::vector<uint8_t>& body);
    
    uint64_t now_ns() const;
};