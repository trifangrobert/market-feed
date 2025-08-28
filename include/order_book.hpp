#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>
#include "wire.hpp"

// -----------------------------------------------------------------------------
// Phase 2: OrderBook interface (single-instrument)
// Goals:
//  - Correctness & clarity first (std::map for sorted prices)
//  - FIFO within each price level
//  - O(1) cancel by id via index map (use std::list for stable iterators)
//  - Emits TradeBody records when matching
// -----------------------------------------------------------------------------

enum class OrderSide : uint8_t { Bid = 0, Ask = 1 };

struct BookOrder {
    uint64_t exch_order_id; // unique within engine
    int32_t  qty;           // remaining quantity
};

// FIFO queue at a given price level. std::list gives stable iterators, so we
// can erase by iterator during cancel without invalidating other iterators.
using LevelQueue = std::list<BookOrder>;

class OrderBook {
public:
    OrderBook() = default;

    // Add a new resting order to the book.
    // Returns false if exch_order_id already exists or qty <= 0.
    bool add_resting(uint64_t exch_order_id,
                   OrderSide side,
                   int64_t price_ticks,
                   int32_t qty);

    // Cancel an existing order by exchange id. Returns false if not found.
    bool cancel_order(uint64_t exch_order_id);

    // Match an incoming (taker) order against the opposite side.
    // Generates one or more TradeBody fills in out_trades; returns total filled qty.
    int32_t match_taker(uint64_t taker_exch_order_id,
                  OrderSide taker_side,
                  int64_t taker_price_ticks,
                  int32_t qty,
                  std::vector<TradeBody>& out_trades,
                  uint32_t instrument_id,
                  uint8_t liquidity_flag /* 0=aggr buy, 1=aggr sell */);

    // Best quotes; return false if that side is empty.
    bool best_bid(int64_t& price_out, int32_t& qty_out) const;
    bool best_ask(int64_t& price_out, int32_t& qty_out) const;

    // Introspection
    size_t num_orders() const { return id_index_.size(); }
    bool empty_bid() const { return bids_.empty(); }
    bool empty_ask() const { return asks_.empty(); }

private:
    using PriceMap = std::map<int64_t, LevelQueue>; // ascending prices

    static OrderSide opposite(OrderSide s) { return s == OrderSide::Bid ? OrderSide::Ask : OrderSide::Bid; }

    bool best_on_side(OrderSide side, int64_t& px, int32_t& qty) const;

    // Internal helpers to access the correct side map
    PriceMap& side_map(OrderSide side) { return (side == OrderSide::Bid) ? bids_ : asks_; }
    const PriceMap& side_map(OrderSide side) const { return (side == OrderSide::Bid) ? bids_ : asks_; }

    static inline bool crossable_test(OrderSide taker_side, int64_t taker_price_ticks, int64_t resting_price_ticks) {
        return (taker_side == OrderSide::Ask) ? (taker_price_ticks <= resting_price_ticks) : (taker_price_ticks >= resting_price_ticks);
    }

    // Containers: bids use max price (best is rbegin), asks use min price (best is begin)
    PriceMap bids_;
    PriceMap asks_;

    // Fast lookup: exch_order_id -> {side, price, iterator into LevelQueue}
    struct IndexEntry {
        OrderSide side;
        int64_t   price_ticks;
        LevelQueue::iterator it; // stable except when element erased
    };
    std::unordered_map<uint64_t, IndexEntry> id_index_;
};