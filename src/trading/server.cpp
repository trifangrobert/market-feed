#include "trading/trading_server.hpp"

static const char* kSockPath = "/tmp/demo.sock";

int main() {
    TradingServer server(kSockPath);
    server.run();
    return 0;
}