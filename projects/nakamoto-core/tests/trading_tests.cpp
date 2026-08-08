#include "trading/FuturesAccount.hpp"
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

    // ================= FuturesAccount (futures accounting model) =================

    { // 8. starting state: 10,000 USDT, flat, everything internally consistent
        nkm::FuturesAccount account;
        expect(approx(account.walletBalance(), 10'000.0), "wallet starts at the starting capital");
        expect(approx(account.availableBalance(), 10'000.0), "available starts at the starting capital");
        expect(approx(account.equity(65'000.0), 10'000.0), "equity = wallet when flat");
        expect(approx(account.position(), 0.0), "flat position");
        expect(approx(account.initialMargin(), 0.0), "no margin locked when flat");
        expect(approx(account.netPnl(65'000.0), 0.0), "net P&L is zero at start");
    }

    { // 9. open 0.10 BTC long at $65,000
        nkm::FuturesAccount account;
        const auto fill = account.openLong(0.10, 65'000.0);
        expect(fill.has_value(), "open long must execute within margin");
        expect(approx(account.position(), 0.10), "position = 0.10 BTC");
        expect(approx(account.averageEntryPrice(), 65'000.0), "entry price = 65,000");
        expect(approx(account.positionNotional(), 6'500.0), "notional ~= 6,500");
        expect(approx(account.initialMargin(), 6'500.0), "margin at 1x ~= 6,500");
        expect(approx(account.tradingFees(), 2.60),
               "taker fee placeholder = 0.04% * 6,500");
        expect(approx(account.walletBalance(), 10'000.0 - 2.60),
               "wallet debited only the fee, NOT the full notional");
        expect(approx(account.availableBalance(), 10'000.0 - 2.60 - 6'500.0),
               "available = wallet - locked initial margin");
        expect(approx(account.equity(65'000.0), 10'000.0 - 2.60),
               "equity = wallet + 0 unrealized at entry mark");
        expect(approx(account.netPnl(65'000.0), -2.60), "net P&L so far = -fee");
    }

    { // 10. mark price up -> unrealized P&L ~= +10 before costs
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);
        expect(approx(account.unrealizedPnl(65'100.0), 10.0),
               "unrealized = (65,100 - 65,000) * 0.10 = +10");
        expect(approx(account.equity(65'100.0), 10'000.0 - 2.60 + 10.0),
               "equity marks the position to market");
    }

    { // 11. close the long at $65,100 -> realized P&L ~= +10 before fees
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);
        (void)account.closeLong(0.10, 65'100.0);
        expect(approx(account.realizedPnl(), 10.0), "realized = (65,100 - 65,000) * 0.10");
        expect(approx(account.position(), 0.0), "flat after close");
        expect(approx(account.unrealizedPnl(65'100.0), 0.0), "no unrealized when flat");
    }

    { // 12. trading fees are deducted separately from P&L
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);  // fee 2.60
        (void)account.closeLong(0.10, 65'100.0); // fee 2.604
        expect(approx(account.tradingFees(), 2.60 + 2.604), "both fill fees accumulate");
        expect(approx(account.walletBalance(), 10'000.0 + 10.0 - 2.60 - 2.604),
               "wallet = start + realized - fees");
        expect(approx(account.netPnl(65'100.0), 10.0 - 2.60 - 2.604),
               "net P&L = gross realized - fees");
    }

    { // 13. loss scenario
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);
        (void)account.closeLong(0.10, 64'900.0); // fee 2.596
        expect(approx(account.realizedPnl(), -10.0), "realized P&L = -10");
        expect(approx(account.walletBalance(), 10'000.0 - 10.0 - 2.60 - 2.596),
               "wallet reflects the loss");
    }

    { // 14. partial close: remaining position and average entry stay correct
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);
        (void)account.closeLong(0.04, 65'100.0);
        expect(approx(account.position(), 0.06), "remaining position 0.06 BTC");
        expect(approx(account.averageEntryPrice(), 65'000.0), "average entry unchanged");
        expect(approx(account.realizedPnl(), 4.0), "realized on the closed 0.04");
        (void)account.closeLong(0.06, 65'200.0);
        expect(approx(account.position(), 0.0), "flat after full close");
        expect(approx(account.realizedPnl(), 4.0 + 12.0), "realized on the rest");
    }

    { // 15. leverage: required initial margin scales as notional / leverage
        nkm::FuturesAccount two;
        two.setLeverage(2.0);
        (void)two.openLong(0.10, 65'000.0);
        expect(approx(two.initialMargin(), 3'250.0), "2x margin = 6,500 / 2");
        expect(approx(two.availableBalance(), 10'000.0 - 3'250.0 - 2.60),
               "more balance stays available at 2x");

        nkm::FuturesAccount five;
        five.setLeverage(5.0);
        (void)five.openLong(0.10, 65'000.0);
        expect(approx(five.initialMargin(), 1'300.0), "5x margin = 6,500 / 5");
        expect(approx(five.leverage(), 5.0), "leverage is stored");
    }

    { // 16. funding payment (simplified, manual application)
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);
        account.setFundingRate(0.0001);
        const double payment = account.applyFunding(65'000.0);
        expect(approx(payment, -0.65), "long pays notional * positive funding rate");
        expect(approx(account.fundingPayments(), -0.65), "funding tracked separately");
        expect(approx(account.walletBalance(), 10'000.0 - 2.60 - 0.65),
               "wallet debited the funding payment");
        expect(approx(account.netPnl(65'000.0), -2.60 - 0.65),
               "net P&L includes funding");

        nkm::FuturesAccount credited;
        (void)credited.openLong(0.10, 65'000.0);
        expect(approx(credited.applyFunding(65'000.0, -0.0001), 0.65),
               "negative funding rate credits the long");
    }

    { // 17. insufficient margin: 0.20 BTC at $65,000 needs $13,000 margin at 1x
        nkm::FuturesAccount account;
        expect(!account.openLong(0.20, 65'000.0).has_value(),
               "order refused when required margin + fee exceed available");
        expect(approx(account.position(), 0.0), "no position opened");

        nkm::FuturesAccount leveraged;
        leveraged.setLeverage(2.0);
        expect(leveraged.openLong(0.20, 65'000.0).has_value(),
               "the same order fits at 2x leverage");
    }

    { // 18. shorts: the signed one-way model supports SELL-to-open cleanly
        nkm::FuturesAccount account;
        (void)account.openShort(0.10, 65'000.0);
        expect(approx(account.position(), -0.10), "short is a negative position");
        expect(approx(account.initialMargin(), 6'500.0), "margin on absolute notional");
        expect(approx(account.unrealizedPnl(64'900.0), 10.0),
               "short profits when the price falls");
        (void)account.closeShort(0.10, 64'900.0);
        expect(approx(account.realizedPnl(), 10.0), "short round trip realized +10");
        expect(approx(account.position(), 0.0), "flat after close");
    }

    { // 19. equity always reconciles with net P&L
        nkm::FuturesAccount account;
        (void)account.openLong(0.10, 65'000.0);
        (void)account.applyFunding(65'050.0, 0.0001);
        (void)account.closeLong(0.04, 65'100.0);
        const double mark = 65'200.0;
        expect(approx(account.equity(mark), account.walletBalance() + account.unrealizedPnl(mark)),
               "equity = wallet + unrealized");
        expect(approx(account.availableBalance(), account.walletBalance() - account.initialMargin()),
               "available = wallet - margin");
        expect(approx(account.netPnl(mark),
                      account.realizedPnl() + account.unrealizedPnl(mark)
                          - account.tradingFees() + account.fundingPayments()),
               "net P&L = realized + unrealized - fees + funding");
        expect(approx(account.equity(mark) - 10'000.0, account.netPnl(mark)),
               "equity - starting capital reconciles with net P&L");
    }

    // ================= PaperExecutionEngine (pipeline execution) =================

    { // 20. buy-to-open / sell-to-close round trip through the engine
        nkm::PaperExecutionEngine engine(nkm::PaperExecutionEngine::Config{
            .startingCapital = 10'000.0,
            .leverage = 1.0,
            .takerFeeRate = 0.01,
            .maxPositionSize = 2.0,
            .tradeQuantity = 1.0,
        });

        const auto buy = engine.tryBuy(100.0, 0.7);
        expect(buy.has_value(), "paper buy must execute");
        expect(buy->side == nkm::Signal::Buy, "trade side must be BUY");
        expect(approx(buy->price, 100.0), "buy fills at the given price");
        expect(approx(buy->quantity, 1.0), "buy fills the configured quantity");
        expect(approx(engine.position(), 1.0), "position +1.0 BTC");
        expect(approx(engine.averageEntryPrice(), 100.0), "entry price = 100");
        expect(approx(engine.initialMargin(), 100.0), "margin = notional / 1x");
        expect(approx(engine.walletBalance(), 10'000.0 - 1.0),
               "wallet debited only the taker fee, not the notional");
        expect(approx(engine.availableBalance(), 10'000.0 - 1.0 - 100.0),
               "available = wallet - locked margin");
        expect(approx(engine.tradingFees(), 1.0), "fee = 1% of 100 notional");
        expect(engine.tradeCount() == 1, "trade count increments");

        const auto sell = engine.trySell(110.0, -0.7);
        expect(sell.has_value(), "paper sell must execute with position");
        expect(approx(engine.position(), 0.0), "flat after close");
        expect(approx(engine.realizedPnl(), 10.0), "realized +10 gross");
        expect(approx(engine.tradingFees(), 1.0 + 1.1), "fees on both fills");
        expect(approx(engine.walletBalance(), 10'000.0 + 10.0 - 1.0 - 1.1),
               "wallet = start + realized - fees");
        expect(approx(engine.netPnl(110.0), 10.0 - 2.1), "net P&L = realized - fees");
    }

    { // 21. SELL with no position is refused (engine is long-only for now)
        nkm::PaperExecutionEngine engine(nkm::PaperExecutionEngine::Config{
            .maxPositionSize = 2.0,
            .tradeQuantity = 1.0,
        });
        expect(!engine.trySell(100.0, -0.7).has_value(),
               "sell with no position must be refused (no shorts in engine)");
        expect(engine.tradeCount() == 0, "refused trade must not count");
    }

    { // 22. max position size cap
        nkm::PaperExecutionEngine engine(nkm::PaperExecutionEngine::Config{
            .startingCapital = 10'000.0,
            .takerFeeRate = 0.0,
            .maxPositionSize = 0.15,
            .tradeQuantity = 0.10,
        });
        expect(engine.tryBuy(65'000.0, 0.7).has_value(), "first buy within cap executes");
        expect(!engine.tryBuy(65'000.0, 0.7).has_value(),
               "buy exceeding the max position must be refused");
        expect(engine.tradeCount() == 1, "refused trade must not count");
    }

    { // 23. insufficient available balance (margin) refuses the buy
        nkm::PaperExecutionEngine engine(nkm::PaperExecutionEngine::Config{
            .startingCapital = 100.0,
            .takerFeeRate = 0.0,
            .maxPositionSize = 10.0,
            .tradeQuantity = 1.0,
        });
        expect(engine.tryBuy(90.0, 0.7).has_value(), "affordable buy executes");
        expect(!engine.tryBuy(90.0, 0.7).has_value(),
               "second buy beyond the available balance must be refused");
    }

    { // 24. average-cost entry across multiple buys
        nkm::PaperExecutionEngine engine(nkm::PaperExecutionEngine::Config{
            .startingCapital = 10'000.0,
            .leverage = 2.0,
            .takerFeeRate = 0.0,
            .maxPositionSize = 2.0,
            .tradeQuantity = 1.0,
        });
        expect(engine.tryBuy(100.0, 0.7).has_value(), "first buy executes");
        expect(engine.tryBuy(120.0, 0.7).has_value(), "second buy executes");
        expect(approx(engine.position(), 2.0), "positions accumulate");
        expect(approx(engine.averageEntryPrice(), 110.0), "average entry (100+120)/2");
    }

    return 0;
}
