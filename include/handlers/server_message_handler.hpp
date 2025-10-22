#pragma once

#include "engine.hpp"
#include "wire.hpp"
#include "codec.hpp"
#include <vector>
#include <chrono>

class ServerMessageHandler {
public:
    explicit ServerMessageHandler(Engine& engine);
    
    std::vector<std::vector<uint8_t>> handle_message(const std::vector<uint8_t>& message);
    
private:
    Engine& engine_;
    uint64_t md_seqno_;
    
    std::vector<std::vector<uint8_t>> handle_new_order(const Header& header, const std::vector<uint8_t>& body);
    std::vector<std::vector<uint8_t>> handle_cancel_order(const Header& header, const std::vector<uint8_t>& body);
    
    uint64_t now_ns() const;
    std::vector<uint8_t> create_ack_message(const AckBody& ack);
    std::vector<uint8_t> create_trade_message(const TradeBody& trade);
};