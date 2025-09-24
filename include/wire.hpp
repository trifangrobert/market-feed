#pragma once
#include <cstdint>
#include <type_traits>

enum class MsgType: uint8_t {
    RESERVED = 0,
    NEW = 1,
    CANCEL = 2,
    ACK = 3, // this one can also be rejected (NACK)
    TRADE = 4,
};

inline constexpr uint8_t kProtocolVersion = 1;
constexpr size_t kMaxFrame = 64 * 1024;
constexpr uint8_t TIF_IOC = 0x1; // bit0: Immediate-Or-Cancel
constexpr uint8_t TIF_FOK = 0x2; // bit1: Fill-Or-Kill

// Naturally aligned header, expected to be 24 bytes on ARM64/x86_64
struct Header {
    uint8_t type; // msg type
    uint8_t version; // protocol version 
    uint16_t size; // header + body bytes
    uint64_t seqno;
    uint64_t ts_ns;
};
static_assert(sizeof(Header) == 24, "Header must be 24 bytes (natural alignment)");
static_assert(std::is_trivially_copyable_v<Header>, "Header must be trivially copyable");

// Naturally aligned. With current field order, will be 32 bytes (rounded up to 8-byte multiple)
struct OrderNewBody {
    uint64_t client_order_id;
    int64_t price_ticks;
    int32_t qty;
    uint32_t instrument_id;
    uint8_t side;
    uint8_t flags;
    uint16_t pad;
};

static_assert(sizeof(OrderNewBody) == 32, "OrderNewBody must be 32 bytes (natural alignment)");
static_assert(std::is_trivially_copyable_v<OrderNewBody>, "OrderNewBody must be trivially copyable");
static_assert(sizeof(Header) + sizeof(OrderNewBody) == 56, "OrderNew message must be 56 bytes (natural alignment)");

struct OrderCancelBody {
    uint64_t exch_order_id; // 0 if unknown
    uint64_t client_order_id; // 0 if unused
    uint32_t instrument_id;
    uint8_t  reason_code; // optional
    // implicit padding here
};

static_assert(sizeof(OrderCancelBody) == 24, "OrderCancelBody must be 24 bytes (natural alignment)");
static_assert(std::is_trivially_copyable_v<OrderCancelBody>, "OrderCancelBody must be trivially copyable");
static_assert(sizeof(Header) + sizeof(OrderCancelBody) == 48, "OrderCancel message must be 48 bytes (natural alignment)");

struct AckBody {
    uint64_t client_order_id;
    uint64_t exch_order_id;     // 0 if NACK and no ID assigned
    uint8_t  status;            // 0=ACK, 1=NACK
    uint8_t  _pad7[7]{};        // align next u64 (explicit for clarity)
    uint64_t ts_engine_recv_ns; // when engine ingested NEW
    uint64_t ts_engine_ack_ns;  // when engine emitted ACK/NACK
};
static_assert(sizeof(AckBody) == 40, "AckBody must be 40 bytes (natural alignment)");
static_assert(std::is_trivially_copyable_v<AckBody>, "AckBody must be trivially copyable");
static_assert(sizeof(Header) + sizeof(AckBody) == 64, "Ack message must be 64 bytes (natural alignment)");

struct TradeBody {
    int64_t  price_ticks;
    int32_t  qty;
    uint8_t  liquidity_flag;    // 0=aggressor buy, 1=aggressor sell
    uint8_t  _pad3[3]{};        // align next u64
    uint64_t resting_exch_order_id; // maker
    uint64_t taking_exch_order_id;  // taker
    uint32_t instrument_id;
    // implicit padding to 40
};
static_assert(sizeof(TradeBody) == 40, "TradeBody must be 40 bytes (natural alignment)");
static_assert(std::is_trivially_copyable_v<TradeBody>, "TradeBody must be trivially copyable");
static_assert(sizeof(Header) + sizeof(TradeBody) == 64, "Trade message must be 64 bytes (natural alignment)");