#include "engine.hpp"
#include <vector>
#include <cassert>

Engine::Engine(uint32_t _instrument_id_) : instrument_id_(_instrument_id_) {}

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
    if (new_order.qty <= 0 || new_order.price_ticks < 0 || new_order.side > 1) {
        AckBody ack = make_ack(new_order.client_order_id, 0, 1, recv_ns, now_ns());
        EngineResult ret{};
        ret.ack = ack;
        return ret;
    }
    uint64_t new_exch_id = allocate_exch_id();
    OrderSide side = static_cast<OrderSide>(new_order.side);

    int32_t remaining = new_order.qty;
    std::vector<TradeBody> trades;
    int32_t filled = book_.match_taker(new_exch_id, side, new_order.price_ticks, remaining, trades, new_order.instrument_id, liq_flag(side));

    remaining -= filled;
    if (rest_leftover && remaining > 0) {
        book_.add_resting(new_exch_id, side, new_order.price_ticks, remaining);
    }

    EngineResult out{};
    out.ack = make_ack(new_order.client_order_id, new_exch_id, 0, recv_ns, now_ns());
    out.trades = std::move(trades);

    return out;
}

EngineResult Engine::on_cancel(const OrderCancelBody& c) {
    const uint64_t recv_ns = now_ns();

    bool ok = book_.cancel_order(c.exch_order_id);

    AckBody ack = make_ack(
        c.client_order_id,
        c.exch_order_id,
        ok ? 0 : 1,
        recv_ns,
        now_ns()
    );

    return EngineResult{ack, {}};
}