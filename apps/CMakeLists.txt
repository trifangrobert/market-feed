add_executable(server server.cpp)
target_link_libraries(server PRIVATE marketfeed_core)

add_executable(trading_server ../src/trading/server.cpp)
target_link_libraries(trading_server PRIVATE marketfeed_core)

add_executable(trading_client ../src/trading/client.cpp)
target_link_libraries(trading_client PRIVATE marketfeed_core)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE marketfeed_core)