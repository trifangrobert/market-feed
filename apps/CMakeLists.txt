add_executable(server server.cpp)
target_link_libraries(server PRIVATE marketfeed_core)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE marketfeed_core)