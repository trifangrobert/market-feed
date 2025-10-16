#include <iostream>

#include "handlers/client_message_handler.hpp"

void ClientMessageHandler::handle_message(const std::vector<uint8_t>& message) {
    if (message.size() < sizeof(Header)) {
        std::cerr << "ServerMessageHandler: Message too small for header\n";
    }

    const Header* header = reinterpret_cast<const Header*>(message.data());
    if (header->size != message.size()) {
        std::cerr << "ServerMessageHandler: Header size mismatch\n";
    }

    std::vector<uint8_t> body(message.begin() + sizeof(Header), message.end());
    switch (static_cast<MsgType>(header->type)) {
        case MsgType::ACK:
            handle_ack(*header, body);
            break;
        case MsgType::TRADE:
            handle_trade(*header, body);
            break;
        default:
            std::cerr << "ClientMessageHandler: Unknown message type: " 
                      << static_cast<int>(header->type) << "\n";
    }
}

void ClientMessageHandler::handle_ack(const Header& header, const std::vector<uint8_t>& body) {
    try {
        auto ack = codec::decode_body<AckBody>(std::span<const uint8_t>(body));
        
        // go gpt, do some printing, at least your are good at this 
        std::cout << "=== ACK RECEIVED ===\n";
        std::cout << "  Client Order ID: " << ack.client_order_id << "\n";
        std::cout << "  Exchange Order ID: " << ack.exch_order_id;
        
        if (ack.exch_order_id == 0) {
            std::cout << " (NACK - Order Rejected)";
        }
        std::cout << "\n";
        
        std::cout << "  Status: ";
        if (ack.status == 0) {
            std::cout << "✅ ACCEPTED\n";
        } else {
            std::cout << "❌ REJECTED (code: " << static_cast<int>(ack.status) << ")\n";
        }
        
        std::cout << "  Engine Recv Time: " << ack.ts_engine_recv_ns << " ns\n";
        std::cout << "  Engine ACK Time:  " << ack.ts_engine_ack_ns << " ns\n";
        
        // Calculate processing latency
        uint64_t processing_latency_ns = ack.ts_engine_ack_ns - ack.ts_engine_recv_ns;
        std::cout << "  Processing Latency: " << processing_latency_ns << " ns";
        if (processing_latency_ns < 1000) {
            std::cout << " (< 1μs) 🚀";
        } else if (processing_latency_ns < 1000000) {
            std::cout << " (" << (processing_latency_ns / 1000.0) << " μs)";
        } else {
            std::cout << " (" << (processing_latency_ns / 1000000.0) << " ms)";
        }
        std::cout << "\n";
        std::cout << "==================\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to decode ACK: " << e.what() << "\n";
    }
}

void ClientMessageHandler::handle_trade(const Header& header, const std::vector<uint8_t>& body) {
    try {
        auto trade = codec::decode_body<TradeBody>(std::span<const uint8_t>(body));
        
        std::cout << "🔥 === TRADE EXECUTED === 🔥\n";
        std::cout << "  Instrument ID: " << trade.instrument_id << "\n";
        std::cout << "  Price: " << trade.price_ticks << " ticks\n";
        std::cout << "  Quantity: " << trade.qty << "\n";
        
        std::cout << "  Side: ";
        if (trade.liquidity_flag == 0) {
            std::cout << "🟢 BUY (Aggressor)\n";
        } else {
            std::cout << "🔴 SELL (Aggressor)\n";
        }
        
        std::cout << "  Maker Order ID: " << trade.resting_exch_order_id << "\n";
        std::cout << "  Taker Order ID: " << trade.taking_exch_order_id << "\n";
        
        // Calculate notional value (assuming price_ticks are in cents or similar)
        double notional = static_cast<double>(trade.price_ticks * trade.qty);
        std::cout << "  Notional Value: " << notional << " (ticks x qty)\n";
        
        std::cout << "  Sequence: " << header.seqno << "\n";
        std::cout << "  Timestamp: " << header.ts_ns << " ns\n";
        std::cout << "========================\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to decode TRADE: " << e.what() << "\n";
    }
}

uint64_t ClientMessageHandler::now_ns() const {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::vector<uint8_t> ClientMessageHandler::create_new_order(const OrderNewBody& new_order) {
    Header new_header = codec::make_header(MsgType::NEW, sizeof(OrderNewBody), 0, now_ns());
    return codec::pack(new_header, new_order);
}

std::vector<uint8_t> ClientMessageHandler::create_cancel_order(const OrderCancelBody& cancel_order) {
    Header cancel_header = codec::make_header(MsgType::CANCEL, sizeof(OrderCancelBody), 0, now_ns());
    return codec::pack(cancel_header, cancel_order);
}