#include "order_book.hpp"
#include <cassert>
#include <iostream>

int main() {
    OrderBook ob;

    // Seed book: two bids at the same price, one ask
    assert(ob.add_resting(1001, OrderSide::Bid, 100, 10));
    assert(ob.add_resting(1002, OrderSide::Bid, 100, 20));
    assert(ob.add_resting(2001, OrderSide::Ask, 101, 5));

    // Basic sanity
    assert(ob.num_orders() == 3);
    assert(!ob.empty_bid());
    assert(!ob.empty_ask());

    // Cancel a valid id on bids (should leave one bid at that level)
    assert(ob.cancel_order(1002) == true);
    assert(ob.num_orders() == 2);
    assert(!ob.empty_bid());
    assert(!ob.empty_ask());

    // Cancel a non-existent id -> false, no state change
    assert(ob.cancel_order(9999) == false);
    assert(ob.num_orders() == 2);

    // Cancel the remaining bid at that level -> bids side becomes empty
    assert(ob.cancel_order(1001) == true);
    assert(ob.num_orders() == 1);
    assert(ob.empty_bid());
    assert(!ob.empty_ask());

    // Cancel the ask -> book fully empty
    assert(ob.cancel_order(2001) == true);
    assert(ob.num_orders() == 0);
    assert(ob.empty_bid());
    assert(ob.empty_ask());

    // Double cancel on the same id should fail
    assert(ob.cancel_order(2001) == false);

    std::cout << "cancel_order_test passed\n";
    return 0;
}