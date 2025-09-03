// tests/engine_new.cpp
#include "engine.hpp"
#include <cassert>
#include <iostream>

int main() {
    Engine eng{1};

    // --- 0) NACK on invalid input (price < 0 allowed? no → reject if < 0)
    {
        OrderNewBody bad{};
        bad.client_order_id = 9000;
        bad.price_ticks = -1;                 // invalid (we allow ==0, reject <0)
        bad.qty = 10;
        bad.instrument_id = 1;
        bad.side = static_cast<uint8_t>(OrderSide::Bid);

        EngineResult r = eng.on_new(bad, /*rest_leftover=*/false);
        assert(r.ack.client_order_id == 9000);
        assert(r.ack.exch_order_id == 0);     // NACK shouldn't allocate an exch id
        assert(r.ack.status == 1);            // NACK
        assert(r.trades.empty());
    }

    // --- 1) Seed resting asks (GTC-like)
    // Ask A1: 30 @ 101
    OrderNewBody a1{};
    a1.client_order_id = 1001;
    a1.price_ticks = 101;
    a1.qty = 30;
    a1.instrument_id = 1;
    a1.side = static_cast<uint8_t>(OrderSide::Ask);

    EngineResult r1 = eng.on_new(a1, /*rest_leftover=*/true);
    assert(r1.ack.status == 0);               // ACK
    assert(r1.ack.exch_order_id != 0);
    assert(r1.trades.empty());

    // Ask A2: 50 @ 102
    OrderNewBody a2{};
    a2.client_order_id = 1002;
    a2.price_ticks = 102;
    a2.qty = 50;
    a2.instrument_id = 1;
    a2.side = static_cast<uint8_t>(OrderSide::Ask);

    EngineResult r2 = eng.on_new(a2, /*rest_leftover=*/true);
    assert(r2.ack.status == 0);
    assert(r2.trades.empty());

    // Best ask should be 101 with head qty 30
    {
        int64_t px; int32_t q;
        assert(eng.best_ask(px, q));
        assert(px == 101);
        assert(q == 30);
    }

    // --- 2) Taker BUY (IOC-like) crosses 101 and part of 102
    OrderNewBody tb{};
    tb.client_order_id = 2001;
    tb.price_ticks = 102;                      // crosses both 101 & 102
    tb.qty = 60;                               // expect 30@101 + 30@102
    tb.instrument_id = 1;
    tb.side = static_cast<uint8_t>(OrderSide::Bid);

    EngineResult r3 = eng.on_new(tb, /*rest_leftover=*/false); // IOC-like
    assert(r3.ack.status == 0);
    assert(r3.trades.size() == 2);

    // trade 1: fill A1
    assert(r3.trades[0].price_ticks == 101);
    assert(r3.trades[0].qty == 30);
    assert(r3.trades[0].liquidity_flag == 0);  // 0 = aggressor buy

    // trade 2: partial A2
    assert(r3.trades[1].price_ticks == 102);
    assert(r3.trades[1].qty == 30);
    assert(r3.trades[1].liquidity_flag == 0);

    // After fills, best ask should be 102 with head qty 20
    {
        int64_t px; int32_t q;
        assert(eng.best_ask(px, q));
        assert(px == 102);
        assert(q == 20);
    }

    std::cout << "engine_new smoke test passed\n";
    return 0;
}