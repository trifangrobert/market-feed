#include "engine.hpp"
#include <cassert>
#include <iostream>

int main() {
    Engine eng{1};

    // --- 1) Cancel non-existent ID -> NACK, echoed exch_order_id ---
    {
        OrderCancelBody c{};
        c.client_order_id = 5001;
        c.exch_order_id   = 999999;   // not present
        c.instrument_id   = 1;

        EngineResult r = eng.on_cancel(c);
        assert(r.ack.client_order_id == 5001);
        assert(r.ack.exch_order_id == 999999);  // echoed back
        assert(r.ack.status == 1);              // NACK
        assert(r.trades.empty());
    }

    // --- 2) Seed a resting ask, then cancel it -> ACK, book updates ---
    // Seed: Ask A1 = 40 @ 101 (GTC-like, so it rests)
    OrderNewBody a1{};
    a1.client_order_id = 6001;
    a1.price_ticks     = 101;
    a1.qty             = 40;
    a1.instrument_id   = 1;
    a1.side            = static_cast<uint8_t>(OrderSide::Ask);

    EngineResult rseed = eng.on_new(a1, /*rest_leftover=*/true);
    assert(rseed.ack.status == 0);
    assert(rseed.ack.exch_order_id != 0);
    assert(rseed.trades.empty());

    // Verify it’s on the book
    {
        int64_t px; int32_t q;
        assert(eng.best_ask(px, q));
        assert(px == 101);
        assert(q == 40);
    }

    // Cancel the seeded order
    OrderCancelBody c2{};
    c2.client_order_id = 6002;
    c2.exch_order_id   = rseed.ack.exch_order_id; // cancel by exch id we just got
    c2.instrument_id   = 1;

    EngineResult rc = eng.on_cancel(c2);
    assert(rc.ack.client_order_id == 6002);
    assert(rc.ack.exch_order_id == rseed.ack.exch_order_id);
    assert(rc.ack.status == 0);                  // ACK
    assert(rc.trades.empty());

    // Book should now have no asks at 101 (empty side)
    {
        int64_t px; int32_t q;
        bool have_ask = eng.best_ask(px, q);
        assert(!have_ask); // side should be empty after the cancel
    }

    std::cout << "engine_cancel test passed\n";
    return 0;
}