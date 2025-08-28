#include "order_book.hpp"
#include <cassert>
#include <iostream>

int main() {
    OrderBook ob;

    // invalid inputs
    assert(!ob.add_resting(1001, OrderSide::Bid, -1, 10));   // negative price
    assert(!ob.add_resting(1002, OrderSide::Ask,  100, 0));  // zero qty
    assert(ob.num_orders() == 0);

    // happy path: two bids at same price (FIFO), one ask
    assert(ob.add_resting(2001, OrderSide::Bid, 100, 10));
    assert(ob.add_resting(2002, OrderSide::Bid, 100, 20));
    assert(ob.add_resting(3001, OrderSide::Ask, 101, 5));
    assert(ob.num_orders() == 3);

    // duplicate id should be rejected
    assert(!ob.add_resting(2001, OrderSide::Ask, 102, 7));
    assert(ob.num_orders() == 3);

    std::cout << "order_book_smoketest passed (num_orders=" 
              << ob.num_orders() << ")\n";
    return 0;
}