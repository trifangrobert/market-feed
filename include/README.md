# Market Feed - Include Directory

This directory contains the core header files that define the market feed system's interfaces and data structures.

## Header Files Overview

### `wire.hpp` - Protocol Definitions
**Purpose**: Defines the binary wire protocol for client-server communication.

**Key Components**:
- **Message Types**: `NEW`, `CANCEL`, `ACK`, `TRADE`, `RESERVED`
- **Protocol Header**: 24-byte aligned message header with type, version, size, sequence number, and timestamp
- **Message Bodies**: 
  - `OrderNewBody` - New order placement (32 bytes)
  - `OrderCancelBody` - Order cancellation request (24 bytes)
  - `AckBody` - Order acknowledgment/rejection (40 bytes)
  - `TradeBody` - Trade execution notification (40 bytes)
- **Protocol Constants**: Version, frame size limits, time-in-force flags (IOC, FOK)

**Design Notes**: All structures are naturally aligned and trivially copyable for efficient serialization.

---

### `codec.hpp` - Message Serialization
**Purpose**: Provides encoding/decoding utilities for converting between C++ structs and binary wire format.

**Key Components**:
- **Header Construction**: `make_header()` for creating protocol headers
- **Serialization**: 
  - `encode<T>()` - Convert struct to byte vector
  - `decode<T>()` - Convert bytes back to struct
  - `pack()` - Combine header + body into complete message
- **Frame Processing**:
  - `unpack_frame()` - Split received data into header + body
  - `decode_body<T>()` - Extract typed body from frame
  - `decode_expected<T>()` - Validate message type and decode

**Usage**: Used by both client and server for all network message processing.

---

### `order_book.hpp` - Order Book Data Structure
**Purpose**: Implements a single-instrument order book with price-time priority matching.

**Key Components**:
- **Core Interface**:
  - `add_resting()` - Add limit order to book
  - `cancel_order()` - Remove order by exchange ID
  - `match_taker()` - Execute market/limit order against book
  - `best_bid()/best_ask()` - Query top of book
- **Data Structures**:
  - `OrderSide` enum (Bid/Ask)
  - `BookOrder` struct (exchange ID + remaining quantity)
  - `LevelQueue` - FIFO queue at each price level
- **Performance Features**:
  - O(1) cancellation via hash map lookup
  - Stable iterators using `std::list`
  - Sorted price levels using `std::map`

**Matching Logic**: Generates `TradeBody` records with maker/taker tracking and liquidity flags.

---

### `engine.hpp` - Trading Engine Interface
**Purpose**: Orchestrates order processing, book management, and response generation.

**Key Components**:
- **Order Processing**:
  - `on_new()` - Process new order requests
  - `on_cancel()` - Process cancellation requests
- **Response Generation**:
  - `EngineResult` - Contains ACK + generated trades
  - Exchange order ID allocation
  - Timestamp tracking for latency measurement
- **Book Integration**:
  - Wraps `OrderBook` with business logic
  - Validates orders before book operations
  - Handles partial fills and resting orders

**Workflow**: NEW order → validation → matching → ACK generation → trade reporting

---

## Usage Patterns

### Typical Message Flow
```cpp
// 1. Server receives binary data
auto frame = codec::unpack_frame(received_bytes);

// 2. Process based on message type
if (frame.hdr.type == MsgType::NEW) {
    auto order = codec::decode_body<OrderNewBody>(frame.body);
    auto result = engine.on_new(order, rest_leftover);
    
    // 3. Send responses
    auto ack_msg = codec::pack(ack_header, result.ack);
    send(ack_msg);
    
    for (const auto& trade : result.trades) {
        auto trade_msg = codec::pack(trade_header, trade);
        send(trade_msg);
    }
}
```

### Order Book Operations
```cpp
OrderBook book;

// Add resting orders
book.add_resting(order_id, OrderSide::Bid, price_ticks, quantity);

// Execute against book
std::vector<TradeBody> trades;
int32_t filled = book.match_taker(taker_id, OrderSide::Ask, price, qty, 
                                  trades, instrument_id, liquidity_flag);

// Query market data
int64_t best_bid_price, best_ask_price;
int32_t best_bid_qty, best_ask_qty;
bool has_bid = book.best_bid(best_bid_price, best_bid_qty);
bool has_ask = book.best_ask(best_ask_price, best_ask_qty);
```

## Design Principles

- **Performance**: Zero-copy operations where possible, cache-friendly data layouts
- **Correctness**: Price-time priority, FIFO matching, atomic operations
- **Simplicity**: Clear interfaces, minimal dependencies, comprehensive error handling
- **Testability**: All components are unit-testable in isolation