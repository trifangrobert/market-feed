#include "codec.hpp"
#include "engine.hpp"
#include <cassert>
#include <iostream>

int main() {
    Header hdr{};
    hdr.type = static_cast<uint8_t>(MsgType::NEW);
    hdr.version = 1;
    hdr.seqno = 0;
    hdr.ts_ns = 123456789;

    OrderNewBody body{};
    body.client_order_id = 42;
    body.price_ticks = 12345; // $123.45 in cents
    body.qty = 100;
    body.instrument_id = 1;
    body.side = 0; // 0 -> bid, 1 -> ask
    body.flags = 0;
    body.pad = 0;

    std::vector<uint8_t> bytes = codec::pack(hdr, body);

    codec::FrameView fv = codec::unpack_frame(bytes);
    assert(fv.hdr.type == static_cast<uint8_t>(MsgType::NEW));
    OrderNewBody unpacked_body = codec::decode_body<OrderNewBody>(fv.body);

    Engine engine;
    OrderNewBody seed{};
    seed.client_order_id = 7;
    seed.price_ticks = body.price_ticks; // same price as bid
    seed.qty = 30;
    seed.instrument_id = 1;
    seed.side = static_cast<uint8_t>(OrderSide::Ask);
    seed.flags = 0;
    EngineResult seed_res = engine.on_new(seed, true);
    assert(seed_res.ack.status == 0);

    EngineResult result = engine.on_new(unpacked_body, false);

    Header ack_hdr{};
    ack_hdr.type = static_cast<uint8_t>(MsgType::ACK);
    ack_hdr.version = 1;
    ack_hdr.seqno = 1;
    ack_hdr.ts_ns = 123456789; 

    std::vector<uint8_t> ack_bytes = codec::pack(ack_hdr, result.ack);
    codec::FrameView fv_ack = codec::unpack_frame(ack_bytes);
    assert(fv_ack.hdr.type == static_cast<uint8_t>(MsgType::ACK));
    AckBody unpacked_ack_body = codec::decode_body<AckBody>(fv_ack.body);
    assert(unpacked_ack_body.client_order_id == body.client_order_id);
    assert(unpacked_ack_body.exch_order_id != 0);
    assert(unpacked_ack_body.status == 0);
    
    uint64_t md_seqno = 1;
    for (auto& trade : result.trades) {
        Header trade_hdr{};
        trade_hdr.type = static_cast<uint8_t>(MsgType::TRADE);
        trade_hdr.version = 1;
        trade_hdr.seqno = ++md_seqno; // increment per trade
        trade_hdr.ts_ns = 123456789;

        std::vector<uint8_t> trade_bytes = codec::pack(trade_hdr, trade);

        codec::FrameView fv_trade = codec::unpack_frame(trade_bytes);
        assert(fv_trade.hdr.type == static_cast<uint8_t>(MsgType::TRADE));
        TradeBody unpacked_trade_body = codec::decode_body<TradeBody>(fv_trade.body);

        assert(unpacked_trade_body.price_ticks == trade.price_ticks);
        assert(unpacked_trade_body.qty == trade.qty);
        assert(unpacked_trade_body.liquidity_flag == trade.liquidity_flag);
        assert(unpacked_trade_body.resting_exch_order_id == trade.resting_exch_order_id);
        assert(unpacked_trade_body.taking_exch_order_id == trade.taking_exch_order_id);

    }
    return 0;
}