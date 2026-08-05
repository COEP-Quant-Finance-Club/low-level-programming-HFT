cmake_minimum_required(VERSION 3.20)

project(NakamotoCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# Trader Executable
# ===========================

add_executable(trader
    apps/trader/main.cpp
)

target_link_libraries(trader
    PRIVATE
        NakamotoLatency
)