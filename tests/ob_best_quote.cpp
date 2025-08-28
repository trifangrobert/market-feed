#include "order_book.hpp"
#include <cassert>
#include <iostream>

int main() {
    OrderBook ob;

    int64_t px; int32_t qty;

    // Empty book -> best_* return false
    assert(!ob.best_bid(px, qty));
    assert(!ob.best_ask(px, qty));

    // Seed bids at 99 (qty 10) and 100 (qty 20)
    assert(ob.add_resting(1001, OrderSide::Bid, 99, 10));
    assert(ob.add_resting(1002, OrderSide::Bid, 100, 20));

    // Seed asks at 101 (qty 30) and 103 (qty 40)
    assert(ob.add_resting(2001, OrderSide::Ask, 101, 30));
    assert(ob.add_resting(2002, OrderSide::Ask, 103, 40));

    // Best bid = 100 (highest bid), head-of-queue qty at that level = 20
    assert(ob.best_bid(px, qty));
    assert(px == 100);
    assert(qty == 20);

    // Best ask = 101 (lowest ask), head-of-queue qty at that level = 30
    assert(ob.best_ask(px, qty));
    assert(px == 101);
    assert(qty == 30);

    // Add another bid at 100 -> FIFO means head qty at best remains 20
    assert(ob.add_resting(1003, OrderSide::Bid, 100, 50));
    assert(ob.best_bid(px, qty));
    assert(px == 100);
    assert(qty == 20);

    // Cancel the head order at best bid (1002) -> head becomes the second FIFO (1003, qty 50)
    assert(ob.cancel_order(1002));
    assert(ob.best_bid(px, qty));
    assert(px == 100);
    assert(qty == 50);

    // Cancel the only ask at 101 -> best ask should move to 103
    assert(ob.cancel_order(2001));
    assert(ob.best_ask(px, qty));
    assert(px == 103);
    assert(qty == 40);

    std::cout << "best_quotes_test passed\n";
    return 0;
}