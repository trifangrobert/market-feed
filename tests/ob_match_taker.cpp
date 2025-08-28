#include "order_book.hpp"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    OrderBook ob;

    // Seed asks: 101:[A1=30, A2=40], 102:[A3=50]
    assert(ob.add_resting(5001, OrderSide::Ask, 101, 30)); // A1
    assert(ob.add_resting(5002, OrderSide::Ask, 101, 40)); // A2
    assert(ob.add_resting(5003, OrderSide::Ask, 102, 50)); // A3
    assert(ob.num_orders() == 3);

    // 1) Match taker BUY @101 for qty=50 -> fills 30@101 (A1) + 20@101 (A2)
    std::vector<TradeBody> trades1;
    int32_t filled1 = ob.match_taker(/*taker_exch_order_id*/9001,
                                     OrderSide::Bid,
                                     /*limit*/101,
                                     /*qty*/50,
                                     trades1,
                                     /*instrument_id*/1,
                                     /*liquidity_flag*/0);
    assert(filled1 == 50);
    assert(trades1.size() == 2);
    // first trade vs A1
    assert(trades1[0].price_ticks == 101);
    assert(trades1[0].qty == 30);
    assert(trades1[0].resting_exch_order_id == 5001);
    assert(trades1[0].taking_exch_order_id  == 9001);
    // second trade vs A2
    assert(trades1[1].price_ticks == 101);
    assert(trades1[1].qty == 20);
    assert(trades1[1].resting_exch_order_id == 5002);
    assert(trades1[1].taking_exch_order_id  == 9001);

    // After the fills: A1 removed, A2 has 20 left at 101, A3 still 50 at 102
    int64_t px; int32_t q;
    assert(ob.best_ask(px, q));
    assert(px == 101);
    assert(q == 20); // head-of-queue is A2 with 20
    assert(ob.num_orders() == 2);

    // 2) Match taker BUY @102 for qty=70 -> fills 20@101 (A2) + 50@102 (A3)
    std::vector<TradeBody> trades2;
    int32_t filled2 = ob.match_taker(/*taker_exch_order_id*/9002,
                                     OrderSide::Bid,
                                     /*limit*/102,
                                     /*qty*/70,
                                     trades2,
                                     /*instrument_id*/1,
                                     /*liquidity_flag*/0);
    assert(filled2 == 70);
    assert(trades2.size() == 2);
    // first trade should finish A2 at 101
    assert(trades2[0].price_ticks == 101);
    assert(trades2[0].qty == 20);
    assert(trades2[0].resting_exch_order_id == 5002);
    assert(trades2[0].taking_exch_order_id  == 9002);
    // second trade should take all of A3 at 102
    assert(trades2[1].price_ticks == 102);
    assert(trades2[1].qty == 50);
    assert(trades2[1].resting_exch_order_id == 5003);
    assert(trades2[1].taking_exch_order_id  == 9002);

    // Book should now have no asks left
    assert(ob.empty_ask());
    assert(ob.num_orders() == 0);

    // 3) Non-marketable: with empty asks, a BUY cannot match
    std::vector<TradeBody> trades3;
    int32_t filled3 = ob.match_taker(/*taker_exch_order_id*/9003,
                                     OrderSide::Bid,
                                     /*limit*/100,
                                     /*qty*/10,
                                     trades3,
                                     /*instrument_id*/1,
                                     /*liquidity_flag*/0);
    assert(filled3 == 0);
    assert(trades3.empty());

    std::cout << "ob_match_taker passed\n";
    return 0;
}
