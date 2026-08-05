cmake_minimum_required(VERSION 3.20)

project(
    NakamotoCore
    VERSION 0.1.0
    DESCRIPTION "Low Latency Crypto Execution Engine"
    LANGUAGES CXX
)

# --------------------------------------------------
# C++ Standard
# --------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------
# Compiler Warnings
# --------------------------------------------------

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# --------------------------------------------------
# Executable
# --------------------------------------------------

add_executable(
    trader
    apps/trader/main.cpp
    src/latency/HighResolutionTimer.cpp
)

target_include_directories(
    trader
    PRIVATE
        include
)