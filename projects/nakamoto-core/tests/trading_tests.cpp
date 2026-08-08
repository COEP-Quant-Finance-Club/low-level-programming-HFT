#include "trading/OrderBookImbalance.hpp"
#include "trading/PaperExecutionEngine.hpp"
#include "trading/Strategy.hpp"

#include "market_data/DepthEvent.hpp"
#include "orderbook/OrderBook.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool approx(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

nkm::OrderBook makeBook(const std::vector<nkm::PriceLevel>& bids,
                        const std::vector<nkm::PriceLevel>& asks)
{
    nkm::OrderBook book;
    nkm::DepthEvent event;
    event.symbol = "BTCUSDT";
    event.bids = bids;
    event.asks = asks;
    book.applyDepthUpdate(event);
    return book;
}

} // namespace

int main()
{
    // ================= OrderBookImbalance =================

    { // 1. perfectly balanced book -> imbalance 0
        nkm::OrderBookImbalance imbalance(5);
        const auto book = makeBook(
            {{100.0, 10.0}, {99.0, 5.0}},
            {{101.0, 5.0}, {102.0, 10.0}});
        expect(approx(imbalance.calculate(book), 0.0), "balanced book must have zero imbalance");
    }

    { // 2. bid-heavy book -> positive imbalance
        nkm::OrderBookImbalance imbalance(5);
        const auto book = makeBook(
            {{100.0, 20.0}, {99.0, 5.0}},
            {{101.0, 5.0}});
        // (25 - 5) / 30 = 2/3
        expect(approx(imbalance.calculate(book), 20.0 / 30.0),
               "bid-heavy book must have positive imbalance");
    }

    { // 3. ask-heavy book -> negative imbalance
        nkm::OrderBookImbalance imbalance(5);
        const auto book = makeBook(
            {{100.0, 5.0}},
            {{101.0, 20.0}, {102.0, 5.0}});
        expect(approx(imbalance.calculate(book), -20.0 / 30.0),
               "ask-heavy book must have negative imbalance");
    }

    { // 4. zero-volume edge case -> 0.0 (no division by zero)
        nkm::OrderBookImbalance imbalance(5);
        const auto book = makeBook({}, {});
        expect(approx(imbalance.calculate(book), 0.0),
               "empty book must yield zero imbalance");
    }

    { // 4b. top-N levels are honored (deep levels outside N are ignored)
        nkm::OrderBookImbalance imbalance(2);
        const auto book = makeBook(
            {{100.0, 10.0}, {99.0, 5.0}, {98.0, 100.0}},
            {{101.0, 5.0}, {102.0, 10.0}, {103.0, 100.0}});
        // Top 2: bids 15, asks 15 -> 0 (the 100-quantity levels at depth 3 are excluded)
        expect(approx(imbalance.calculate(book), 0.0),
               "levels deeper than N must not affect the imbalance");
    }

    // ================= ThresholdStrategy =================

    { // 5. BUY threshold
        nkm::ThresholdStrategy strategy(0.60);
        expect(strategy.evaluate(0.60) == nkm::Signal::Buy, "imbalance at +threshold must be BUY");
        expect(strategy.evaluate(1.00) == nkm::Signal::Buy, "imbalance above +threshold must be BUY");
    }

    { // 6. SELL threshold
        nkm::ThresholdStrategy strategy(0.60);
        expect(strategy.evaluate(-0.60) == nkm::Signal::Sell, "imbalance at -threshold must be SELL");
        expect(strategy.evaluate(-1.00) == nkm::Signal::Sell, "imbalance below -threshold must be SELL");
    }

    { // 7. HOLD region
        nkm::ThresholdStrategy strategy(0.60);
        expect(strategy.evaluate(0.0) == nkm::Signal::Hold, "balanced imbalance must HOLD");
        expect(strategy.evaluate(0.59) == nkm::Signal::Hold, "below +threshold must HOLD");
        expect(strategy.evaluate(-0.59) == nkm::Signal::Hold, "above -threshold must HOLD");
    }

    // ================= PaperExecutionEngine =================

    { // 8. paper BUY
        nkm::PaperExecutionEngine engine(10'000.0, 0.01, 2.0, 1.0);
        const auto trade = engine.tryBuy(100.0, 0.7);
        expect(trade.has_value(), "paper buy must execute within position limit");
        expect(trade->side == nkm::Signal::Buy, "trade side must be BUY");
        expect(approx(trade->price, 100.0), "buy must fill at the given price");
        expect(approx(trade->quantity, 1.0), "buy must fill the configured quantity");
        expect(approx(engine.position(), 1.0), "position must increase");
        expect(approx(engine.averageEntryPrice(), 100.0), "entry price must be the fill price");
        expect(approx(engine.cash(), 10'000.0 - 100.0 - 1.0), "cash must debit notional + fee");
        expect(approx(engine.feesPaid(), 1.0), "fee must be notional * fee_rate");
        expect(engine.tradeCount() == 1, "trade count must increment");
    }

    { // 9. paper SELL
        nkm::PaperExecutionEngine engine(10'000.0, 0.01, 2.0, 1.0);
        expect(engine.tryBuy(100.0, 0.7).has_value(), "buy should execute");
        const auto trade = engine.trySell(110.0, -0.7);
        expect(trade.has_value(), "paper sell must execute with sufficient position");
        expect(trade->side == nkm::Signal::Sell, "trade side must be SELL");
        expect(approx(trade->price, 110.0), "sell must fill at the given price");
        expect(approx(engine.position(), 0.0), "position must return to zero");
        expect(approx(engine.cash(), 10'000.0 - 101.0 + 110.0 - 1.1),
               "cash must credit notional minus fee");
    }

    { // 10. fees
        nkm::PaperExecutionEngine engine(10'000.0, 0.02, 2.0, 1.0);
        expect(engine.tryBuy(100.0, 0.7).has_value(), "buy should execute"); // fee 2.0
        expect(engine.trySell(100.0, -0.7).has_value(), "sell should execute"); // fee 2.0
        expect(approx(engine.feesPaid(), 4.0), "fees must accumulate per side");
    }

    { // 11. position limit
        nkm::PaperExecutionEngine engine(10'000.0, 0.0, 1.0, 1.0);
        expect(engine.tryBuy(100.0, 0.7).has_value(), "first buy within limit must execute");
        expect(!engine.tryBuy(100.0, 0.7).has_value(),
               "second buy exceeding max position must be refused");
        expect(engine.tradeCount() == 1, "refused trade must not count");

        // Selling more than the position is refused (no shorting).
        nkm::PaperExecutionEngine noShort(10'000.0, 0.0, 1.0, 1.0);
        expect(!noShort.trySell(100.0, -0.7).has_value(),
               "sell with no position must be refused");

        // Buying beyond available cash is refused (no leverage).
        nkm::PaperExecutionEngine noCash(100.0, 0.0, 10.0, 1.0);
        expect(noCash.tryBuy(90.0, 0.7).has_value(), "affordable buy must execute");
        expect(!noCash.tryBuy(90.0, 0.7).has_value(),
               "buy exceeding remaining cash must be refused");
    }

    { // 12. realized P&L
        nkm::PaperExecutionEngine engine(10'000.0, 0.0, 2.0, 1.0);
        expect(engine.tryBuy(100.0, 0.7).has_value(), "buy should execute");
        expect(engine.trySell(110.0, -0.7).has_value(), "sell should execute");
        expect(approx(engine.realizedPnl(), 10.0), "realized P&L = (110-100)*1");
        expect(approx(engine.totalPnl(110.0), 10.0), "total P&L must equal realized when flat");
    }

    { // 13. unrealized P&L
        nkm::PaperExecutionEngine engine(10'000.0, 0.0, 2.0, 1.0);
        expect(engine.tryBuy(100.0, 0.7).has_value(), "buy should execute");
        expect(approx(engine.unrealizedPnl(110.0), 10.0), "unrealized P&L = (110-100)*1");
        expect(approx(engine.totalPnl(110.0), 10.0), "total P&L must include unrealized");
    }

    { // average-cost entry across multiple buys
        nkm::PaperExecutionEngine engine(10'000.0, 0.0, 2.0, 1.0);
        expect(engine.tryBuy(100.0, 0.7).has_value(), "first buy should execute");
        expect(engine.tryBuy(120.0, 0.7).has_value(), "second buy should execute");
        expect(approx(engine.position(), 2.0), "two buys accumulate position");
        expect(approx(engine.averageEntryPrice(), 110.0), "average entry must be (100+120)/2");
    }

    return 0;
}
