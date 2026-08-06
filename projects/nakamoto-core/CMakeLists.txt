cmake_minimum_required(VERSION 3.20)

project(NakamotoCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ubuntu prerequisites:
#   sudo apt update
#   sudo apt install -y build-essential cmake libssl-dev libboost-all-dev

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

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
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

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
)