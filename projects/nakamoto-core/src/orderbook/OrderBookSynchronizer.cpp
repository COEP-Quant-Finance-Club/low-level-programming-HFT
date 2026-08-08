#include "orderbook/OrderBookSynchronizer.hpp"

namespace nkm {

void OrderBookSynchronizer::onDepthEvent(const DepthEvent& event)
{
    // A pending snapshot may be waiting for the first event to bridge it.
    processPendingSnapshot();

    if (status_.load(std::memory_order_acquire) == BookStatus::Synchronized) {
        applyNext(event);
    }
    else {
        // Buffering or ResyncNeeded: keep the event for the next sync attempt.
        buffer_.push_back(event);
        processPendingSnapshot();
    }
}

void OrderBookSynchronizer::submitSnapshot(std::shared_ptr<const OrderBookSnapshot> snapshot)
{
    pendingSnapshot_.store(std::move(snapshot), std::memory_order_release);
}

void OrderBookSynchronizer::processPendingSnapshot()
{
    auto snapshot = pendingSnapshot_.load(std::memory_order_acquire);
    if (!snapshot) {
        return;
    }

    // Attempt synchronization. If the attempt needs more stream events, the
    // snapshot stays pending and is retried on the next depth event. The
    // snapshot is only cleared once it has been settled; a snapshot submitted
    // concurrently by the supervision thread is never wiped out (compare_exchange
    // only clears the pointer we actually processed).
    const SyncOutcome outcome = synchronizeFrom(*snapshot);
    if (outcome == SyncOutcome::Completed) {
        std::shared_ptr<const OrderBookSnapshot> expected = snapshot;
        pendingSnapshot_.compare_exchange_strong(expected, nullptr,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
    }
}

OrderBookSynchronizer::SyncOutcome OrderBookSynchronizer::synchronizeFrom(const OrderBookSnapshot& snapshot)
{
    const std::uint64_t lastUpdateId = snapshot.lastUpdateId;

    // 1. Discard buffered events that are definitively older than the
    //    snapshot (final update ID u <= snapshot lastUpdateId).
    std::size_t staleCount = 0;
    while (staleCount < buffer_.size() && buffer_[staleCount].finalUpdateId <= lastUpdateId) {
        ++staleCount;
    }
    if (staleCount > 0) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(staleCount));
    }

    if (buffer_.empty()) {
        // No event bridges the snapshot point yet: the next stream event is
        // expected to bridge it (its range starts at or before lastUpdateId+1).
        // Keep the snapshot pending and retry on the next depth event.
        status_.store(BookStatus::Buffering, std::memory_order_release);
        return SyncOutcome::KeepWaiting;
    }

    const DepthEvent& bridge = buffer_.front();

    // 2. The first processed event must satisfy U <= lastUpdateId+1 AND
    //    u >= lastUpdateId+1. If not, the stream has moved past the snapshot
    //    point (U > lastUpdateId+1): this snapshot can never bridge, so it is
    //    discarded and a fresh snapshot must be requested.
    if (!(bridge.firstUpdateId <= lastUpdateId + 1 && bridge.finalUpdateId >= lastUpdateId + 1)) {
        status_.store(BookStatus::ResyncNeeded, std::memory_order_release);
        return SyncOutcome::Completed;
    }

    // 3. Rebuild the book from the snapshot, then apply the bridge event and
    //    every subsequent buffered event in sequence.
    book_.applySnapshot(snapshot.bids, snapshot.asks);
    lastAppliedU_.store(bridge.finalUpdateId, std::memory_order_release);
    book_.applyDepthUpdate(bridge);

    // The bridge anchors the book to the live stream, so synchronization is
    // provisionally complete; a gap during the buffered replay below flips the
    // status back to ResyncNeeded.
    status_.store(BookStatus::Synchronized, std::memory_order_release);

    std::size_t consumed = 1; // the bridge event
    for (; consumed < buffer_.size(); ++consumed) {
        applyNext(buffer_[consumed]);
        if (status_.load(std::memory_order_acquire) == BookStatus::ResyncNeeded) {
            break;
        }
    }
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(consumed));

    return SyncOutcome::Completed;
}

void OrderBookSynchronizer::applyNext(const DepthEvent& event)
{
    // 4. USDⓈ-M Futures depth events may have gaps between consecutive events,
    //    so continuity is validated with the 'pu' (previous final update ID)
    //    field: each new event's pu must equal the previous event's u.
    //    pu > previous u means events were missed; pu < previous u means the
    //    data is out of order. Either way the book can no longer be trusted.
    const std::uint64_t expectedPreviousU = lastAppliedU_.load(std::memory_order_acquire);
    if (event.previousFinalUpdateId == expectedPreviousU) {
        book_.applyDepthUpdate(event);
        lastAppliedU_.store(event.finalUpdateId, std::memory_order_release);
        return;
    }

    status_.store(BookStatus::ResyncNeeded, std::memory_order_release);
}

BookStatus OrderBookSynchronizer::status() const
{
    return status_.load(std::memory_order_acquire);
}

bool OrderBookSynchronizer::isSynchronized() const
{
    return status() == BookStatus::Synchronized;
}

std::uint64_t OrderBookSynchronizer::lastUpdateId() const
{
    return lastAppliedU_.load(std::memory_order_acquire);
}

const OrderBook& OrderBookSynchronizer::book() const
{
    return book_;
}

} // namespace nkm
