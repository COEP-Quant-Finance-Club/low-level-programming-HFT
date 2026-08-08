#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

#include "exchange/BinanceFuturesClient.hpp"
#include "latency/Benchmark.hpp"
#include "latency/HighResolutionTimer.hpp"
#include "market_data/BinanceOrderBookSnapshot.hpp"
#include "orderbook/OrderBookSynchronizer.hpp"
#include "trading/OrderBookImbalance.hpp"
#include "trading/PaperExecutionEngine.hpp"
#include "trading/Strategy.hpp"

namespace {

constexpr std::string_view kSymbol = "BTCUSDT";

// --- Paper trading configuration (all assumptions, clearly labeled) ---
// Starting paper capital in USDT.
constexpr double kStartingCash = 10'000.0;
// Flat proportional fee on notional per side. Conservative placeholder only -
// NOT a real Binance fee tier; configurable assumption.
constexpr double kFeeRate = 0.0002; // 0.02%
// Maximum absolute position in base units (BTC). Trades exceeding it are refused.
constexpr double kMaxPositionSize = 1.0;
// Fixed base quantity per paper trade.
constexpr double kTradeQuantity = 0.05;
// Imbalance threshold for the baseline strategy (+/-).
constexpr double kImbalanceThreshold = 0.60;
// Number of top price levels used by the imbalance signal.
constexpr std::size_t kImbalanceLevels = 5;
// Status print interval.
constexpr std::chrono::seconds kStatusInterval{2};

std::string_view signalName(nkm::Signal signal)
{
    switch (signal) {
        case nkm::Signal::Buy:
            return "BUY";
        case nkm::Signal::Sell:
            return "SELL";
        case nkm::Signal::Hold:
            return "HOLD";
    }
    return "?";
}

void printPaperStatus(const nkm::OrderBookSynchronizer& synchronizer,
                      const nkm::OrderBookImbalance& imbalanceSignal,
                      const nkm::ThresholdStrategy& strategy,
                      const nkm::PaperExecutionEngine& engine)
{
    const auto& book = synchronizer.book();
    const double imbalance = imbalanceSignal.calculate(book);
    const nkm::Signal signal = strategy.evaluate(imbalance);
    const double markPrice = book.midPrice();

    std::cout << "========================================\n";
    std::cout << "Nakamoto Core - Paper Trader\n";
    std::cout << "========================================\n\n";
    std::cout << "Symbol: " << kSymbol << "\n\n";

    std::cout << "Book:\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Best Bid: " << book.bestBid() << "\n";
    std::cout << "  Best Ask: " << book.bestAsk() << "\n";
    std::cout << "  Spread  : " << book.spread() << "\n\n";

    std::cout << "Signal:\n";
    std::cout << std::showpos << std::setprecision(2) << "  Imbalance: " << imbalance << "\n";
    std::cout << std::noshowpos << "  Signal   : " << signalName(signal) << "\n\n";

    std::cout << "Paper Account:\n";
    std::cout << "  Cash           : " << engine.cash() << "\n";
    std::cout << "  Position       : " << engine.position() << "\n";
    std::cout << "  Avg Entry      : " << engine.averageEntryPrice() << "\n";
    std::cout << "  Unrealized P&L : " << engine.unrealizedPnl(markPrice) << "\n";
    std::cout << "  Realized P&L   : " << engine.realizedPnl() << "\n";
    std::cout << "  Fees Paid      : " << engine.feesPaid() << "\n";
    std::cout << "  Trades         : " << engine.tradeCount() << "\n";
    std::cout << "  Equity (m2m)   : " << engine.equity(markPrice) << "\n\n";
    std::cout << "========================================\n";
    std::cout << std::flush;
}

void printPaperTrade(const nkm::PaperTrade& trade)
{
    std::cout << "[PAPER TRADE]\n";
    std::cout << signalName(trade.side) << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Price    : " << trade.price << "\n";
    std::cout << "Quantity : " << trade.quantity << "\n";
    std::cout << "Imbalance: " << trade.imbalance << "\n";
    std::cout << "Fee      : " << trade.fee << "\n";
    std::cout << std::flush;
}

} // namespace

int main()
{
    // --- Sanity check: HighResolutionTimer against a known sleep duration.
    // Not a benchmark - a smoke test that the timer reports ~100ms.
    nkm::HighResolutionTimer timer;

    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.stop();

    std::cout << "Sanity check - HighResolutionTimer\n"
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

    if (!client.subscribeDepth(std::string(kSymbol)))
    {
        std::cerr << "Failed to subscribe to " << kSymbol << " depth" << std::endl;
        return 1;
    }

    nkm::BinanceOrderBookSnapshot snapshotClient;
    nkm::OrderBookSynchronizer synchronizer;

    // --- Paper trading pipeline (single-threaded, no real execution) ---
    //   DepthEvent -> Synchronizer -> OrderBook -> Imbalance -> Strategy
    //              -> PaperExecutionEngine -> P&L
    nkm::OrderBookImbalance imbalanceSignal(kImbalanceLevels);
    nkm::ThresholdStrategy strategy(kImbalanceThreshold);
    nkm::PaperExecutionEngine engine(kStartingCash, kFeeRate, kMaxPositionSize, kTradeQuantity);

    // ------------------------------------------------------------------
    // The WebSocket receive loop runs on a background thread and feeds
    // depth events to the synchronizer. Once the book is synchronized, the
    // trading pipeline runs inline on that same thread (single-threaded
    // after the market-data event reaches the pipeline).
    // ------------------------------------------------------------------
    auto lastStatusPrint = std::chrono::steady_clock::now();
    std::thread streamThread([&] {
        client.run([&](const nkm::DepthEvent& event) {
            synchronizer.onDepthEvent(event);

            if (synchronizer.isSynchronized())
            {
                const auto& book = synchronizer.book();
                const double imbalance = imbalanceSignal.calculate(book);
                const nkm::Signal signal = strategy.evaluate(imbalance);

                if (signal == nkm::Signal::Buy)
                {
                    if (const auto trade = engine.tryBuy(book.bestAsk(), imbalance)) {
                        printPaperTrade(*trade);
                    }
                }
                else if (signal == nkm::Signal::Sell)
                {
                    if (const auto trade = engine.trySell(book.bestBid(), imbalance)) {
                        printPaperTrade(*trade);
                    }
                }
            }

            // Print a compact status periodically, not on every update.
            const auto now = std::chrono::steady_clock::now();
            if (now - lastStatusPrint >= kStatusInterval) {
                printPaperStatus(synchronizer, imbalanceSignal, strategy, engine);
                lastStatusPrint = now;
            }
        });

        std::cerr << "DEBUG: WebSocket receive loop ended - market data stopped\n";
    });

    // Fetch the REST snapshot after the stream has started so that depth
    // events arriving between subscription and snapshot are buffered.
    std::cout << "DEBUG: Fetching REST order book snapshot...\n";
    auto snapshot = snapshotClient.fetch(kSymbol);
    while (!snapshot)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        snapshot = snapshotClient.fetch(kSymbol);
    }
    std::cout << "DEBUG: REST snapshot received (lastUpdateId="
              << snapshot->lastUpdateId << ")\n";

    synchronizer.submitSnapshot(
        std::make_shared<const nkm::OrderBookSnapshot>(std::move(*snapshot)));

    // Supervision loop: whenever a sequence gap invalidates the book,
    // re-fetch the snapshot and re-submit it for resynchronization.
    while (true)
    {
        if (synchronizer.status() == nkm::BookStatus::ResyncNeeded)
        {
            std::cout << "DEBUG: Sequence gap detected - re-fetching REST snapshot...\n";
            auto fresh = snapshotClient.fetch(kSymbol);
            if (fresh)
            {
                synchronizer.submitSnapshot(
                    std::make_shared<const nkm::OrderBookSnapshot>(std::move(*fresh)));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Unreachable: run until interrupted.
    streamThread.join();
    return 0;
}