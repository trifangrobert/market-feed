#include "trading/trading_client.hpp"
#include "order_book.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

static const char* kSockPath = "/tmp/demo.sock";

int main() {
    try {
        std::cout << "Creating trading client 2...\n";
        TradingClient client(kSockPath);
        
        std::cout << "Placing SELL order for AAPL...\n";
        // Place a sell order for AAPL: 50 shares at $150.00 (15000 ticks)
        uint64_t exchange_order_id = client.place_order(
            1,                    // instrument_id (1 = AAPL)
            OrderSide::Ask,       // side (Ask = sell)
            15000,                // price_ticks ($150.00)
            50                   // quantity
        );
        
        std::cout << "Order placed successfully! Exchange Order ID: " << exchange_order_id << "\n";
        
        std::cout << "Keeping connection open for potential trades (waiting 30 seconds)...\n";
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}