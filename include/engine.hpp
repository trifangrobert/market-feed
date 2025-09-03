#pragma once
#include <vector>
#include "order_book.hpp"
#include "wire.hpp"

struct EngineResult {
    AckBody ack;
    std::vector<TradeBody> trades;
};

class Engine {
public:
    explicit Engine(uint32_t instrument_id = 1);
    EngineResult on_new(const OrderNewBody& new_order, bool rest_leftover);
    EngineResult on_cancel(const OrderCancelBody& cancel);
    bool best_bid(int64_t& price_out, int32_t& qty_out) const {return book_.best_bid(price_out, qty_out); }
    bool best_ask(int64_t& price_out, int32_t& qty_out) const {return book_.best_ask(price_out, qty_out); }
private:
    uint64_t next_exch_id_ = 1;
    uint32_t instrument_id_ = 1;

    OrderBook book_; // we will eventually convert this to a map <instrument_id, order_book>
    uint64_t allocate_exch_id() { return next_exch_id_++; }
    static uint8_t liq_flag(OrderSide side) { return side == OrderSide::Bid ? 0 : 1; } 

    static uint64_t now_ns() noexcept;
    static AckBody make_ack(uint64_t client_id, uint64_t exch_id, uint8_t status, uint64_t recv_ns, uint64_t ack_ns);
};