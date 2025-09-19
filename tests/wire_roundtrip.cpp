#include "../include/wire.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <cstring>

int main() {
    Header hdr{};
    hdr.type = static_cast<uint8_t>(MsgType::NEW);
    hdr.version = 1;
    hdr.size = sizeof(Header) + sizeof(OrderNewBody);
    hdr.seqno = 0;
    hdr.ts_ns = 123456789;

    OrderNewBody body{};
    body.client_order_id = 42;
    body.price_ticks = 12345; // $123.45 in cents
    body.qty = 100;
    body.instrument_id = 1; // AAPL
    body.side = 0; // 0 -> bid, 1 -> ask
    body.flags = 0;
    body.pad = 0;

    std::array<uint8_t, sizeof(Header) + sizeof(OrderNewBody)> buffer{};
    std::memcpy(buffer.data(), &hdr, sizeof(Header));
    std::memcpy(buffer.data() + sizeof(Header), &body, sizeof(OrderNewBody));

    Header hdr_copy{};
    OrderNewBody body_copy{};

    std::memcpy(&hdr_copy, buffer.data(), sizeof(Header));
    std::memcpy(&body_copy, buffer.data() + sizeof(Header), sizeof(OrderNewBody));

    assert(hdr_copy.ts_ns == hdr.ts_ns);
    assert(body_copy.client_order_id == body.client_order_id);
    assert(body_copy.price_ticks == body.price_ticks);
    assert(body_copy.qty == body.qty);

    std::cout << "Round-trip successful! Header+OrderNew is " << buffer.size() << " bytes\n";
    return 0;
}