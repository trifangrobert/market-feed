cmake_minimum_required(VERSION 3.16)
project(market_feed LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings (portable)
if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core static library built from src/core/*.cpp
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.cpp")
if (NOT CORE_SOURCES)
  message(FATAL_ERROR "No sources found in src/core/*.cpp — expected at least one.")
endif()

# Trading server library built from src/trading/*.cpp, src/network/*.cpp, and src/handlers/*.cpp
file(GLOB TRADING_SOURCES CONFIGURE_DEPENDS "src/trading/*.cpp" "src/network/*.cpp" "src/handlers/*.cpp")

# Combine all sources
set(ALL_SOURCES ${CORE_SOURCES} ${TRADING_SOURCES})

add_library(marketfeed_core STATIC ${ALL_SOURCES})
target_include_directories(marketfeed_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_features(marketfeed_core PUBLIC cxx_std_20)

add_subdirectory(apps)

# Enable tests and pull them in
include(CTest)            # BUILD_TESTING is ON by default after this
if (BUILD_TESTING)
  add_subdirectory(tests) # expects tests/CMakeLists.txt
endif()