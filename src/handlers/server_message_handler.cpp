#include <iostream>
#include <span>

#include "handlers/server_message_handler.hpp"

ServerMessageHandler::ServerMessageHandler(Engine& engine) 
    : engine_(engine), md_seqno_(0) {
}

std::vector<std::vector<uint8_t>> ServerMessageHandler::handle_message(const std::vector<uint8_t>& message) {
    if (message.size() < sizeof(Header)) {
        std::cerr << "ServerMessageHandler: Message too small for header\n";
        return {};
    }
    
    const Header* header = reinterpret_cast<const Header*>(message.data());
    
    if (header->size != message.size()) {
        std::cerr << "ServerMessageHandler: Header size mismatch\n";
        return {};
    }
    
    // Extract body
    std::vector<uint8_t> body(message.begin() + sizeof(Header), message.end());
    
    switch (static_cast<MsgType>(header->type)) {
        case MsgType::NEW:
            return handle_new_order(*header, body);
        case MsgType::CANCEL:
            return handle_cancel_order(*header, body);
        case MsgType::ACK:
        case MsgType::TRADE:
            std::cout << "ServerMessageHandler: Received " << static_cast<int>(header->type) 
                      << " (not expected from client)\n";
            break;
        case MsgType::RESERVED:
            std::cout << "ServerMessageHandler: Received RESERVED message (not expected from client)\n";
            break;
        default:
            std::cerr << "ServerMessageHandler: Unknown message type: " 
                      << static_cast<int>(header->type) << "\n";
            break;
    }
    
    return {};
}

std::vector<std::vector<uint8_t>> ServerMessageHandler::handle_new_order(const Header& header, const std::vector<uint8_t>& body) {
    try {
        auto new_order = codec::decode_body<OrderNewBody>(std::span<const uint8_t>(body));
        
        std::cout << "NEW: cid=" << new_order.client_order_id
                  << " side=" << int(new_order.side)
                  << " qty=" << new_order.qty
                  << " px=" << new_order.price_ticks
                  << " instr=" << new_order.instrument_id
                  << " flags=0x" << std::hex << int(new_order.flags) << std::dec << "\n";
        
        bool rest_leftover = ((new_order.flags & TIF_IOC) == 0);
        EngineResult result = engine_.on_new(new_order, rest_leftover);
        
        std::vector<std::vector<uint8_t>> responses;
        
        // send the ack
        responses.push_back(create_ack_message(result.ack));
        
        // send the trades
        for (const auto& trade : result.trades) {
            responses.push_back(create_trade_message(trade));
        }
        
        return responses;
        
    } catch (const std::exception& e) {
        std::cerr << "ServerMessageHandler: Failed to decode NEW order: " << e.what() << "\n";
        return {};
    }
}

std::vector<std::vector<uint8_t>> ServerMessageHandler::handle_cancel_order(const Header& header, const std::vector<uint8_t>& body) {
    try {
        auto cancel_order = codec::decode_body<OrderCancelBody>(std::span<const uint8_t>(body));
        
        std::cout << "CANCEL: cid=" << cancel_order.client_order_id << " exch_order_id=" << cancel_order.exch_order_id << " instr=" << cancel_order.instrument_id << "\n";
        
        EngineResult result = engine_.on_cancel(cancel_order);
        
        // send ack
        std::vector<std::vector<uint8_t>> responses;
        responses.push_back(create_ack_message(result.ack));
        
        return responses;
        
    } catch (const std::exception& e) {
        std::cerr << "ServerMessageHandler: Failed to decode CANCEL order: " << e.what() << "\n";
        return {};
    }
}

uint64_t ServerMessageHandler::now_ns() const {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::vector<uint8_t> ServerMessageHandler::create_ack_message(const AckBody& ack) {
    Header ack_header = codec::make_header(MsgType::ACK, sizeof(AckBody), 0, now_ns());
    return codec::pack(ack_header, ack);
}

std::vector<uint8_t> ServerMessageHandler::create_trade_message(const TradeBody& trade) {
    Header trade_header = codec::make_header(MsgType::TRADE, sizeof(TradeBody), ++md_seqno_, now_ns());
    return codec::pack(trade_header, trade);
}