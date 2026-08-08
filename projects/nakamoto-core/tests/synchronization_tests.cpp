#include "market_data/BinanceOrderBookSnapshot.hpp"
#include "orderbook/OrderBookSynchronizer.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

nkm::DepthEvent makeEvent(std::uint64_t firstUpdateId,
                          std::uint64_t finalUpdateId,
                          std::uint64_t previousFinalUpdateId,
                          std::vector<nkm::PriceLevel> bids = {},
                          std::vector<nkm::PriceLevel> asks = {})
{
    nkm::DepthEvent event;
    event.eventTimestamp = 1;
    event.symbol = "BTCUSDT";
    event.firstUpdateId = firstUpdateId;
    event.finalUpdateId = finalUpdateId;
    event.previousFinalUpdateId = previousFinalUpdateId;
    event.bids = std::move(bids);
    event.asks = std::move(asks);
    return event;
}

std::shared_ptr<const nkm::OrderBookSnapshot> makeSnapshot(std::uint64_t lastUpdateId,
                                                           std::vector<nkm::PriceLevel> bids,
                                                           std::vector<nkm::PriceLevel> asks)
{
    nkm::OrderBookSnapshot snapshot;
    snapshot.lastUpdateId = lastUpdateId;
    snapshot.bids = std::move(bids);
    snapshot.asks = std::move(asks);
    return std::make_shared<const nkm::OrderBookSnapshot>(std::move(snapshot));
}

} // namespace

int main()
{
    // ---- 11. Snapshot initialization ----
    {
        nkm::OrderBookSynchronizer sync;

        // Bridge event: U=1000, u=1010 (pu irrelevant for the bridge) updates
        // bid 100.0 and deletes ask 101.0.
        sync.onDepthEvent(makeEvent(1000, 1010, 0,
                                    {{100.0, 11.0}},
                                    {{101.0, 0.0}}));

        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}, {99.0, 5.0}}, {{101.0, 7.0}}));
        sync.processPendingSnapshot();

        expect(sync.isSynchronized(), "snapshot + bridge should synchronize the book");
        expect(sync.book().bidQuantity(100.0) == 11.0, "bridge event should update snapshot level");
        expect(sync.book().bidQuantity(99.0) == 5.0, "snapshot bid level should be initialized");
        expect(sync.book().askQuantity(101.0) == 0.0, "bridge event should delete snapshot ask");
        expect(sync.book().bestBid() == 100.0, "best bid should reflect synchronized book");
    }

    // ---- 12. Buffered event older than the snapshot is discarded ----
    {
        nkm::OrderBookSynchronizer sync;

        // Stale event (u=999 <= lastUpdateId=1000) must never be applied.
        sync.onDepthEvent(makeEvent(995, 999, 0, {{100.0, 1.0}}, {}));
        sync.onDepthEvent(makeEvent(1000, 1010, 0, {}, {{102.0, 3.0}}));

        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}}, {{101.0, 5.0}}));
        sync.processPendingSnapshot();

        expect(sync.isSynchronized(), "sync should succeed after dropping stale events");
        expect(sync.book().bidQuantity(100.0) == 10.0,
               "stale pre-snapshot event must not be applied to the book");
        expect(sync.book().askQuantity(102.0) == 3.0, "bridging event should be applied");
    }

    // ---- 13. First valid bridging event (U <= lastUpdateId+1 AND u >= lastUpdateId+1) ----
    {
        nkm::OrderBookSynchronizer sync;

        // Event A is stale; event B (U=1000,u=1010) bridges the snapshot (S=1000);
        // event C (pu=1010, u=1020) is a subsequent sequential event.
        sync.onDepthEvent(makeEvent(995, 999, 0, {{99.0, 1.0}}, {}));       // A - stale
        sync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 12.0}}, {}));   // B - bridge
        sync.onDepthEvent(makeEvent(1015, 1020, 1010, {{99.0, 2.0}}, {}));  // C - subsequent

        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}}, {}));
        sync.processPendingSnapshot();

        expect(sync.isSynchronized(), "bridge + subsequent events should synchronize");
        expect(sync.book().bidQuantity(100.0) == 12.0, "bridge event B should apply");
        expect(sync.book().bidQuantity(99.0) == 2.0, "subsequent event C should apply");
        expect(sync.lastUpdateId() == 1020, "last update ID should track applied stream");
    }

    // ---- 14. Subsequent sequential events after synchronization ----
    {
        nkm::OrderBookSynchronizer sync;

        sync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 10.0}}, {}));
        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}, {99.0, 5.0}}, {}));
        sync.processPendingSnapshot();
        expect(sync.isSynchronized(), "initial sync should complete");

        // Futures events chain via pu == previous event's u (U may jump).
        sync.onDepthEvent(makeEvent(1011, 1020, 1010, {{100.0, 12.0}}, {}));
        expect(sync.isSynchronized(), "sequential event should keep the book synchronized");
        expect(sync.book().bidQuantity(100.0) == 12.0, "sequential event should apply");

        sync.onDepthEvent(makeEvent(1030, 1040, 1020, {{99.0, 6.0}}, {}));
        expect(sync.book().bidQuantity(99.0) == 6.0, "second sequential event should apply");
        expect(sync.lastUpdateId() == 1040, "last update ID should advance");
    }

    // ---- 15. Sequence gap detection ----
    {
        nkm::OrderBookSynchronizer sync;

        sync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 10.0}}, {}));
        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}}, {}));
        sync.processPendingSnapshot();
        expect(sync.isSynchronized(), "initial sync should complete");

        // Gap: pu=1500 is far beyond the previous u=1010 -> events were missed.
        sync.onDepthEvent(makeEvent(1501, 1600, 1500, {{100.0, 99.0}}, {}));

        expect(sync.status() == nkm::BookStatus::ResyncNeeded,
               "sequence gap must mark the book as needing resync");
        expect(!sync.isSynchronized(), "book must not claim synchronization after a gap");

        // Out-of-order/overlapping event (pu < previous u) must also trigger resync.
        nkm::OrderBookSynchronizer overlapSync;
        overlapSync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 10.0}}, {}));
        overlapSync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}}, {}));
        overlapSync.processPendingSnapshot();
        overlapSync.onDepthEvent(makeEvent(1005, 1012, 1004, {{100.0, 7.0}}, {}));
        expect(overlapSync.status() == nkm::BookStatus::ResyncNeeded,
               "out-of-order overlapping event must trigger resync");
    }

    // ---- 16a. KeepWaiting: snapshot with no bridging event buffered yet ----
    {
        nkm::OrderBookSynchronizer sync;

        // Snapshot arrives before any bridging event: the attempt must keep
        // the snapshot pending, not discard it (regression: the book used to
        // get stuck in BUFFERING forever when the buffer was empty).
        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}}, {{101.0, 5.0}}));
        sync.processPendingSnapshot();

        expect(!sync.isSynchronized(), "empty buffer must not synchronize yet");
        expect(sync.status() == nkm::BookStatus::Buffering,
               "sync attempt with no bridging event must keep waiting");

        // The next live event bridges the still-pending snapshot.
        sync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 11.0}}, {}));

        expect(sync.isSynchronized(), "next event should bridge the retained snapshot");
        expect(sync.book().bidQuantity(100.0) == 11.0,
               "bridge event should apply on top of the snapshot");
        expect(sync.book().bidQuantity(99.0) == 0.0, "snapshot level untouched by bridge");
    }

    // ---- 16b. Snapshot too old to bridge is discarded and triggers resync ----
    {
        nkm::OrderBookSynchronizer sync;

        // The buffered stream is far past the snapshot point (U=2000 > S+1):
        // this snapshot can never bridge and must be discarded.
        sync.onDepthEvent(makeEvent(2000, 2010, 0, {{100.0, 9.0}}, {}));
        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 1.0}}, {}));
        sync.processPendingSnapshot();

        expect(sync.status() == nkm::BookStatus::ResyncNeeded,
               "snapshot too old must trigger resync");
        expect(!sync.isSynchronized(), "book must not be synchronized with a stale snapshot");

        // The discarded snapshot must not linger: more events cannot sync it.
        sync.onDepthEvent(makeEvent(2011, 2020, 2010, {}, {}));
        expect(sync.status() == nkm::BookStatus::ResyncNeeded,
               "discarded snapshot must not be retried");
    }

    // ---- 16c. Resync where the fresh snapshot is newer than all buffered events ----
    {
        nkm::OrderBookSynchronizer sync;

        sync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 10.0}}, {}));
        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}}, {}));
        sync.processPendingSnapshot();
        expect(sync.isSynchronized(), "initial sync should complete");

        // Gap invalidates the book.
        sync.onDepthEvent(makeEvent(1501, 1600, 1500, {{100.0, 99.0}}, {}));
        expect(sync.status() == nkm::BookStatus::ResyncNeeded, "gap should be detected");

        // Post-gap event is buffered, but the fresh snapshot is newer than it.
        sync.onDepthEvent(makeEvent(1601, 1700, 1600, {{100.0, 50.0}}, {}));
        sync.submitSnapshot(makeSnapshot(1700, {{100.0, 1.0}}, {}));
        sync.processPendingSnapshot();

        expect(sync.status() == nkm::BookStatus::Buffering,
               "all buffered events older than the fresh snapshot are dropped; keep waiting");
        expect(!sync.isSynchronized(), "must wait for an event that bridges the fresh snapshot");

        // The next live event bridges and resynchronizes the book.
        sync.onDepthEvent(makeEvent(1701, 1800, 1700, {{100.0, 55.0}}, {}));

        expect(sync.isSynchronized(), "next live event should resynchronize the book");
        expect(sync.book().bidQuantity(100.0) == 55.0,
               "resynced book should reflect the live stream, not the stale one");
        expect(sync.lastUpdateId() == 1800, "last update ID should reflect the resync");
    }

    // ---- 16. Resynchronization behavior ----
    {
        nkm::OrderBookSynchronizer sync;

        sync.onDepthEvent(makeEvent(1000, 1010, 0, {{100.0, 10.0}}, {}));
        sync.submitSnapshot(makeSnapshot(1000, {{100.0, 10.0}, {99.0, 5.0}}, {}));
        sync.processPendingSnapshot();
        expect(sync.isSynchronized(), "initial sync should complete");

        // Sequence gap invalidates the book.
        sync.onDepthEvent(makeEvent(1501, 1600, 1500, {{100.0, 99.0}}, {}));
        expect(sync.status() == nkm::BookStatus::ResyncNeeded, "gap should be detected");

        // Events received after the gap are buffered for the next sync attempt.
        sync.onDepthEvent(makeEvent(1601, 1700, 1600, {{100.0, 50.0}}, {{103.0, 8.0}}));

        // A fresh snapshot (S=1600) bridges to the buffered post-gap event.
        sync.submitSnapshot(makeSnapshot(1600, {{100.0, 1.0}}, {{103.0, 2.0}}));
        sync.processPendingSnapshot();

        expect(sync.isSynchronized(), "fresh snapshot should resynchronize the book");
        expect(sync.book().bidQuantity(100.0) == 50.0,
               "buffered post-gap event should apply after resync");
        expect(sync.book().askQuantity(103.0) == 8.0,
               "buffered post-gap ask should apply after resync");
        expect(sync.book().bestBid() == 100.0, "book should be rebuilt from fresh snapshot");
        expect(sync.lastUpdateId() == 1700, "last update ID should reflect resync");
    }

    return 0;
}
