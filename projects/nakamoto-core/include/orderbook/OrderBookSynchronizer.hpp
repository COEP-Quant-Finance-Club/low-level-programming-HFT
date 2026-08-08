#pragma once

#include "market_data/BinanceOrderBookSnapshot.hpp"
#include "orderbook/OrderBook.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace nkm {

enum class BookStatus
{
    Buffering,     // collecting depth events before the snapshot is applied
    Synchronized,  // book is live and consistent with the stream
    ResyncNeeded,  // sequence gap detected; book invalid; fresh snapshot required
};

// Combines a REST snapshot with buffered WebSocket depth events to build a
// synchronized local L2 order book, following the Binance USDⓈ-M Futures
// update-ID synchronization algorithm.
//
// Threading: onDepthEvent() is called from the WebSocket thread and owns the
// event buffer and the OrderBook. submitSnapshot() may be called from any
// thread; the snapshot is handed over atomically and consumed by the
// WebSocket thread on the next depth event (or via processPendingSnapshot()).
// No mutexes are used; the order book hot path stays single-threaded.
class OrderBookSynchronizer
{
public:
    OrderBookSynchronizer() = default;
    ~OrderBookSynchronizer() = default;

    OrderBookSynchronizer(const OrderBookSynchronizer&) = delete;
    OrderBookSynchronizer& operator=(const OrderBookSynchronizer&) = delete;
    OrderBookSynchronizer(OrderBookSynchronizer&&) = delete;
    OrderBookSynchronizer& operator=(OrderBookSynchronizer&&) = delete;

    // Feed one WebSocket depth event (WebSocket thread).
    void onDepthEvent(const DepthEvent& event);

    // Hand over a REST snapshot; consumed on the next depth event.
    void submitSnapshot(std::shared_ptr<const OrderBookSnapshot> snapshot);

    // Try to advance synchronization using a pending snapshot. Called
    // automatically after buffering events; public for tests.
    void processPendingSnapshot();

    [[nodiscard]] BookStatus status() const;
    [[nodiscard]] bool isSynchronized() const;

    // Last applied final update ID (u).
    [[nodiscard]] std::uint64_t lastUpdateId() const;

    // Current accumulated market state (valid only while synchronized).
    [[nodiscard]] const OrderBook& book() const;

private:
    // Whether a synchronization attempt settled the snapshot or must keep it
    // pending until an event that bridges the snapshot point arrives.
    enum class SyncOutcome
    {
        Completed,    // snapshot consumed: synchronized, or unusable (resync needed)
        KeepWaiting,  // no bridging event buffered yet; retry on the next event
    };

    SyncOutcome synchronizeFrom(const OrderBookSnapshot& snapshot);
    void applyNext(const DepthEvent& event);

    std::atomic<BookStatus> status_{BookStatus::Buffering};
    std::atomic<std::uint64_t> lastAppliedU_{0};
    std::atomic<std::shared_ptr<const OrderBookSnapshot>> pendingSnapshot_{};
    std::vector<DepthEvent> buffer_;
    OrderBook book_;
};

} // namespace nkm
