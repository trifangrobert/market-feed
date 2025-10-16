#include "trading/trading_client.hpp"
#include "order_book.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

static const char* kSockPath = "/tmp/demo.sock";

int main() {
    try {
        std::cout << "Creating trading client...\n";
        TradingClient client(kSockPath);
        
        std::cout << "Placing order for AAPL...\n";
        // Place a buy order for AAPL: 100 shares at $150.50 (15050 ticks, assuming 1 tick = $0.01)
        uint64_t exchange_order_id = client.place_order(
            1,                    // instrument_id (1 = AAPL)
            OrderSide::Bid,       // side (Bid = buy)
            15050,                // price_ticks ($150.50)
            100                   // quantity
        );
        
        std::cout << "Order placed successfully! Exchange Order ID: " << exchange_order_id << "\n";
        
        // std::cout << "Cancelling order...\n";
        // bool cancelled = client.cancel_order(exchange_order_id, 1);
        // if (cancelled) {
        //     std::cout << "Order cancelled successfully!\n";
        // } else {
        //     std::cout << "Order cancellation failed or order was already filled.\n";
        // }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}