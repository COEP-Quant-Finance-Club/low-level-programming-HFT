#include <cstdint>
#include <iostream>
#include <thread>

#include "exchange/BinanceFuturesClient.hpp"
#include "latency/Benchmark.hpp"
#include "latency/HighResolutionTimer.hpp"

int main()
{
    // --- Sanity check: HighResolutionTimer against a known sleep duration.
    // Not a benchmark — a smoke test that the timer reports ~100ms.
    nkm::HighResolutionTimer timer;

    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.stop();

    std::cout << "Sanity check — HighResolutionTimer\n"
              << "  Elapsed: " << timer.elapsedMilliseconds() << " ms\n\n";

    // --- Baseline: harness overhead floor.
    nkm::run_benchmark(
        "baseline_empty",
        1'000'000,
        []() noexcept {})
        .print();

    std::cout << '\n';

    // --- Trivial capturing lambda, standing in for future benchmarks.
    std::uint64_t counter = 0;
    nkm::run_benchmark(
        "counter_increment",
        1'000'000,
        [&counter]() noexcept { ++counter; })
        .print();

    std::cout << '\n';

    // --- Benchmark the timer itself, using the benchmark harness.
    nkm::HighResolutionTimer bench_timer;
    nkm::run_benchmark(
        "HighResolutionTimer_start_stop",
        1'000'000,
        [&bench_timer]() noexcept {
            bench_timer.start();
            bench_timer.stop();
        })
        .print();

    std::cout << "\nConnecting to Binance Futures Testnet...\n";

    nkm::BinanceFuturesClient client;
    if (!client.connect())
    {
        std::cerr << "Failed to connect to Binance Futures Testnet" << std::endl;
        return 1;
    }

    if (!client.subscribeTrades("BTCUSDT"))
    {
        std::cerr << "Failed to subscribe to BTCUSDT trades" << std::endl;
        return 1;
    }

    client.run();
    return 0;
}