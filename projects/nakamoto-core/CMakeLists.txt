cmake_minimum_required(VERSION 3.20)

project(NakamotoCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ubuntu prerequisites:
#   sudo apt update
#   sudo apt install -y build-essential cmake libssl-dev libboost-all-dev libsimdjson-dev
#
# simdjson was chosen because it is a SIMD-accelerated, single-header-ish, low-overhead
# JSON library that parses the Binance JSON frames directly into view-based structures
# without the weight of a full DOM object model. The parser is intentionally minimal and
# production-friendly for the latency-sensitive market-data boundary.

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(simdjson CONFIG REQUIRED)

# ===========================
# Latency Library
# ===========================

add_library(NakamotoLatency
    src/latency/HighResolutionTimer.cpp
)

target_include_directories(NakamotoLatency
    PUBLIC
        include
)

# ===========================
# Networking Library
# ===========================

add_library(NakamotoNetworking
    src/networking/TcpSocket.cpp
)

target_include_directories(NakamotoNetworking
    PUBLIC
        include
)

# ===========================
# Market Data Library
# ===========================

add_library(NakamotoMarketData
    src/market_data/MarketDataParser.cpp
    src/market_data/BinanceOrderBookSnapshot.cpp
)

target_include_directories(NakamotoMarketData
    PUBLIC
        include
)

target_link_libraries(NakamotoMarketData
    PUBLIC
        simdjson
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

# ===========================
# Order Book Library
# ===========================

add_library(NakamotoOrderBook
    src/orderbook/OrderBook.cpp
    src/orderbook/OrderBookSynchronizer.cpp
)

target_include_directories(NakamotoOrderBook
    PUBLIC
        include
)

target_link_libraries(NakamotoOrderBook
    PUBLIC
        NakamotoMarketData
)

# ===========================
# Trading Library
# ===========================

add_library(NakamotoTrading
    src/trading/OrderBookImbalance.cpp
    src/trading/Strategy.cpp
    src/trading/FuturesAccount.cpp
    src/trading/PaperExecutionEngine.cpp
)

target_include_directories(NakamotoTrading
    PUBLIC
        include
)

target_link_libraries(NakamotoTrading
    PUBLIC
        NakamotoOrderBook
)

# ===========================
# Exchange Library
# ===========================

add_library(NakamotoExchange
    src/exchange/BinanceFuturesClient.cpp
)

target_include_directories(NakamotoExchange
    PUBLIC
        include
)

target_link_libraries(NakamotoExchange
    PUBLIC
        NakamotoMarketData
        NakamotoOrderBook
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

# ===========================
# Tests
# ===========================

enable_testing()
add_executable(orderbook_tests
    tests/orderbook_tests.cpp
)

target_link_libraries(orderbook_tests
    PRIVATE
        NakamotoOrderBook
)

add_test(NAME orderbook_tests COMMAND orderbook_tests)

add_executable(synchronization_tests
    tests/synchronization_tests.cpp
)

target_link_libraries(synchronization_tests
    PRIVATE
        NakamotoOrderBook
)

add_test(NAME synchronization_tests COMMAND synchronization_tests)

add_executable(trading_tests
    tests/trading_tests.cpp
)

target_link_libraries(trading_tests
    PRIVATE
        NakamotoTrading
)

add_test(NAME trading_tests COMMAND trading_tests)

# ===========================
# Trader Executable
# ===========================

add_executable(trader
    apps/trader/main.cpp
)

target_link_libraries(trader
    PRIVATE
        NakamotoLatency
        NakamotoNetworking
        NakamotoExchange
        NakamotoTrading
)
