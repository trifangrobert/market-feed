#include "engine.hpp"
#include <vector>
#include <cassert>
#include <chrono>
#include <string>

Engine::Engine() : next_exch_id_(1), next_instrument_id_(1) {
    // dummy data for now
    add_new_instrument("AAPL");
    add_new_instrument("MSFT");
    add_new_instrument("META");
};

bool Engine::instrument_exists(uint32_t instrument_id) const {
    return order_books.contains(instrument_id);
}

uint32_t Engine::add_new_instrument(const std::string& instrument_name) {
    uint32_t new_id = next_instrument_id_++;
    order_books[new_id] = OrderBook();
    id_to_ticker[new_id] = instrument_name;
    return new_id;
}

bool Engine::best_bid(const uint32_t instrument_id, int64_t& price_out, int32_t& qty_out) const {
    if (!instrument_exists(instrument_id)) {
        return false;
    }
    return order_books.at(instrument_id).best_bid(price_out, qty_out);
}

bool Engine::best_ask(const uint32_t instrument_id, int64_t& price_out, int32_t& qty_out) const {
    if (!instrument_exists(instrument_id)) {
        return false;
    }
    return order_books.at(instrument_id).best_ask(price_out, qty_out);
}

uint64_t Engine::now_ns() noexcept {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

AckBody Engine::make_ack(uint64_t client_id, uint64_t exch_id, uint8_t status, uint64_t recv_ns, uint64_t ack_ns) {
    AckBody a{};
    a.client_order_id = client_id;
    a.exch_order_id = exch_id;
    a.status = status;
    a.ts_engine_recv_ns = recv_ns;
    a.ts_engine_ack_ns = ack_ns;
    return a;
}

EngineResult Engine::on_new(const OrderNewBody& new_order, bool rest_leftover) {
    const uint64_t recv_ns = now_ns();
    if (new_order.qty <= 0 || new_order.price_ticks < 0 || new_order.side > 1 || !instrument_exists(new_order.instrument_id)) {
        AckBody ack = make_ack(new_order.client_order_id, 0, 1, recv_ns, now_ns());
        EngineResult ret{};
        ret.ack = ack;
        return ret;
    }
    OrderBook& order_book = order_books[new_order.instrument_id];
    uint64_t new_exch_id = allocate_exch_id();
    OrderSide side = static_cast<OrderSide>(new_order.side);

    int32_t remaining = new_order.qty;
    std::vector<TradeBody> trades;
    int32_t filled = order_book.match_taker(new_exch_id, side, new_order.price_ticks, remaining, trades, new_order.instrument_id, liq_flag(side));

    remaining -= filled;
    if (rest_leftover && remaining > 0) {
        order_book.add_resting(new_exch_id, side, new_order.price_ticks, remaining);
    }

    EngineResult out{};
    out.ack = make_ack(new_order.client_order_id, new_exch_id, 0, recv_ns, now_ns());
    out.trades = std::move(trades);

    return out;
}

EngineResult Engine::on_cancel(const OrderCancelBody& cancel_order) {
    const uint64_t recv_ns = now_ns();
    if (!instrument_exists(cancel_order.instrument_id)) {
        AckBody ack = make_ack(cancel_order.client_order_id, 0, 1, recv_ns, now_ns());
        EngineResult ret{};
        ret.ack = ack;
        return ret;
    }

    OrderBook& order_book = order_books[cancel_order.instrument_id];
    bool ok = order_book.cancel_order(cancel_order.exch_order_id);

    AckBody ack = make_ack(
        cancel_order.client_order_id,
        cancel_order.exch_order_id,
        ok ? 0 : 1,
        recv_ns,
        now_ns()
    );

    return EngineResult{ack, {}};
}