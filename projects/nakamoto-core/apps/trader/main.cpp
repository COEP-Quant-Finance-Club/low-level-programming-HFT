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
// Starting paper capital in USDT (collateral for the futures account).
constexpr double kStartingCapital = 10'000.0;
// Simplified paper margin model: initial margin = position notional / leverage.
constexpr double kLeverage = 1.0;
// Fee placeholders - NOT verified Binance fee tiers; to be researched before
// real capital. Current execution is taker (BUY at ask, SELL at bid), so the
// taker rate is charged on fills; the maker rate is stored for a future
// limit-order path.
constexpr double kMakerFeeRate = 0.0002; // 0.02% placeholder
constexpr double kTakerFeeRate = 0.0004; // 0.04% placeholder
// Simplified funding placeholder: per-interval rate; applied manually only,
// never fetched from a live funding API yet.
constexpr double kFundingRate = 0.0001; // 0.01% placeholder
constexpr std::chrono::hours kFundingInterval{8};
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

    const double pos = engine.position();
    std::string_view side = "FLAT";
    if (pos > 1e-9) {
        side = "LONG";
    } else if (pos < -1e-9) {
        side = "SHORT";
    }

    std::cout << "========================================\n";
    std::cout << "Nakamoto Core - Paper Futures Account\n";
    std::cout << "========================================\n\n";
    std::cout << "Symbol: BTCUSDT Perpetual\n\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Wallet Balance : " << engine.walletBalance() << " USDT\n";
    std::cout << "Available      : " << engine.availableBalance() << " USDT\n";
    std::cout << "Equity         : " << engine.equity(markPrice) << " USDT\n\n";

    std::cout << "Position       : " << std::showpos << std::setprecision(4) << pos << " BTC\n";
    std::cout << "Position Side  : " << side << '\n';
    std::cout << std::noshowpos << std::setprecision(2) << "\n";

    std::cout << "Mark Price     : " << markPrice << '\n';
    std::cout << "Entry Price    : " << engine.averageEntryPrice() << "\n\n";

    std::cout << "Position Notional : " << engine.positionNotional() << " USDT\n";
    std::cout << "Initial Margin    : " << engine.initialMargin() << " USDT\n";
    std::cout << "Leverage          : " << engine.leverage() << "x\n\n";

    std::cout << "Unrealized P&L : " << engine.unrealizedPnl(markPrice) << " USDT\n";
    std::cout << "Realized P&L   : " << engine.realizedPnl() << " USDT\n\n";

    std::cout << "Trading Fees  : " << engine.tradingFees() << " USDT\n";
    std::cout << "Funding       : " << engine.fundingPayments() << " USDT\n";
    std::cout << "Net P&L       : " << engine.netPnl(markPrice) << " USDT\n\n";

    std::cout << "Trades        : " << engine.tradeCount() << "\n\n";

    std::cout << "--- Market / Signal ---\n";
    std::cout << "Best Bid  : " << book.bestBid() << '\n';
    std::cout << "Best Ask  : " << book.bestAsk() << '\n';
    std::cout << "Spread    : " << book.spread() << '\n';
    std::cout << "Imbalance : " << std::showpos << imbalance << '\n';
    std::cout << std::noshowpos << "Signal    : " << signalName(signal) << '\n';
    std::cout << "========================================\n";
    std::cout << std::flush;
}

void printPaperTrade(const nkm::PaperTrade& trade)
{
    std::cout << "[PAPER TRADE]\n";
    std::cout << signalName(trade.side) << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Price        : " << trade.price << "\n";
    std::cout << "Quantity     : " << trade.quantity << "\n";
    std::cout << "Imbalance    : " << trade.imbalance << "\n";
    std::cout << "Fee (taker)  : " << trade.fee << "\n";
    std::cout << "Available    : " << trade.availableAfter << " USDT\n";
    std::cout << "Margin       : " << trade.marginAfter << " USDT\n";
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
    nkm::PaperExecutionEngine engine(nkm::PaperExecutionEngine::Config{
        .startingCapital = kStartingCapital,
        .leverage = kLeverage,
        .makerFeeRate = kMakerFeeRate,
        .takerFeeRate = kTakerFeeRate,
        .fundingRate = kFundingRate,
        .fundingInterval = kFundingInterval,
        .maxPositionSize = kMaxPositionSize,
        .tradeQuantity = kTradeQuantity,
    });

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