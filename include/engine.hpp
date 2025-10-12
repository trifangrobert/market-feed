#pragma once
#include <vector>
#include <unordered_map>
#include <string>

#include "order_book.hpp"
#include "wire.hpp"

struct EngineResult {
    AckBody ack;
    std::vector<TradeBody> trades;
};

class Engine {
public:
    Engine();
    EngineResult on_new(const OrderNewBody& new_order, bool rest_leftover);
    EngineResult on_cancel(const OrderCancelBody& cancel);
    bool best_bid(const uint32_t instrument_id, int64_t& price_out, int32_t& qty_out) const;
    bool best_ask(const uint32_t instrument_id, int64_t& price_out, int32_t& qty_out) const;
    uint32_t add_new_instrument(const std::string& instrument_name);
private:
    std::unordered_map<uint32_t, OrderBook> order_books;
    std::unordered_map<uint32_t, std::string> id_to_ticker; 
    uint64_t next_exch_id_ = 1;
    uint32_t next_instrument_id_ = 1;

    uint64_t allocate_exch_id() { return next_exch_id_++; }
    static uint8_t liq_flag(OrderSide side) { return side == OrderSide::Bid ? 0 : 1; } 

    static uint64_t now_ns() noexcept;
    static AckBody make_ack(uint64_t client_id, uint64_t exch_id, uint8_t status, uint64_t recv_ns, uint64_t ack_ns);
    bool instrument_exists(uint32_t instrument_id) const;
};