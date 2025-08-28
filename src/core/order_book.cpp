#include "order_book.hpp"
#include <vector>
#include <cassert>

bool OrderBook::add_resting(uint64_t exch_order_id, OrderSide side, int64_t price_ticks, int32_t qty) {
    if (qty <= 0 || price_ticks < 0) [[unlikely]] {
        return false;
    }
    if (id_index_.count(exch_order_id)) {
        return false;
    }

    PriceMap& price_map = side_map(side);
    
    auto [level_it, inserted] = price_map.try_emplace(price_ticks, LevelQueue{});
    LevelQueue& q = level_it->second;

    q.push_back(BookOrder{exch_order_id, qty});
    auto it_order = std::prev(q.end());
    auto [_, ok] = id_index_.try_emplace(exch_order_id, IndexEntry{side, price_ticks, it_order});
    if (!ok) {
        // Roll back: remove the order we just pushed
        q.pop_back();
        if (q.empty() && inserted) {
            price_map.erase(level_it);
        }
        assert(false && "id_index_ emplace failed unexpectedly");
        return false;
    }

    return true;
}

bool OrderBook::cancel_order(uint64_t exch_order_id) {
    auto it_idx = id_index_.find(exch_order_id);
    if (it_idx == id_index_.end()) {
        return false;
    }

    const IndexEntry entry = it_idx->second;

    PriceMap& pm = side_map(entry.side);
    auto lvl_it = pm.find(entry.price_ticks);
    if (lvl_it == pm.end()) {
        assert(false && "cancel_order: index points to missing level");
        id_index_.erase(it_idx);
        return false;
    }

    LevelQueue& q = lvl_it->second;
    q.erase(entry.it);
    if (q.empty()) {
        pm.erase(lvl_it);
    }

    id_index_.erase(it_idx);
    return true;
}

bool OrderBook::best_ask(int64_t& price_out, int32_t& qty_out) const {
    const PriceMap& pm = side_map(OrderSide::Ask);
    if (pm.empty()) {
        return false;
    }

    const auto it = pm.begin();
    price_out = it->first;

    const LevelQueue& q = it->second;
    qty_out = q.empty() ? 0 : q.front().qty;

    return true;
}

bool OrderBook::best_bid(int64_t& price_out, int32_t& qty_out) const {
    const PriceMap& pm = side_map(OrderSide::Bid);
    if (pm.empty()) {
        return false;
    }

    const auto it = pm.rbegin();
    price_out = it->first;
    
    const LevelQueue& q = it->second;
    qty_out = q.empty() ? 0 : q.front().qty;

    return true;
}


bool OrderBook::best_on_side(OrderSide side, int64_t& px, int32_t& qty) const {
    return side == OrderSide::Ask ? OrderBook::best_ask(px, qty) : OrderBook::best_bid(px, qty);
}

int32_t OrderBook::match_taker(uint64_t taker_exch_order_id,
                  OrderSide taker_side,
                  int64_t taker_price_ticks,
                  int32_t qty,
                  std::vector<TradeBody>& out_trades,
                  uint32_t instrument_id,
                  uint8_t liquidity_flag) {
    if (qty <= 0 || taker_price_ticks < 0) [[unlikely]] {
        return 0;
    }

    PriceMap& resting_pm = side_map(opposite(taker_side));
    const OrderSide resting_side = opposite(taker_side);
    int32_t filled_qty = 0;
    while (qty > 0) {
        int64_t resting_price_ticks;
        int32_t resting_qty;
        if (!best_on_side(resting_side, resting_price_ticks, resting_qty)) {
            break;
        }

        if (!crossable_test(taker_side, taker_price_ticks, resting_price_ticks)) {
            break;
        }

        LevelQueue& level_queue = resting_pm.find(resting_price_ticks)->second;
        while (qty > 0 && !level_queue.empty()) {
            BookOrder& resting_order = level_queue.front();

            int32_t traded_qty = std::min(qty, resting_order.qty);
            qty -= traded_qty;
            resting_order.qty -= traded_qty;
            filled_qty += traded_qty;

            TradeBody t{};
            t.price_ticks = resting_price_ticks;
            t.qty = traded_qty;
            t.liquidity_flag = liquidity_flag;
            t.resting_exch_order_id = resting_order.exch_order_id; // maker
            t.taking_exch_order_id  = taker_exch_order_id;         // taker
            t.instrument_id = instrument_id;
            out_trades.push_back(t);

            if (resting_order.qty == 0) {
                id_index_.erase(resting_order.exch_order_id);
                level_queue.pop_front();
            }
        }
        if (level_queue.empty()) {
            resting_pm.erase(resting_price_ticks);
        }
    }
    return filled_qty;
}