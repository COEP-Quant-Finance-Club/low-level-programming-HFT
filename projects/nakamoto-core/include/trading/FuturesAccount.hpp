#pragma once

#include <chrono>
#include <optional>

namespace nkm {

// Result of a simulated futures fill (pure accounting, no exchange contact).
struct FuturesFill
{
    double price{};
    double quantity{};        // absolute quantity filled (BTC)
    double fee{};             // taker fee paid on this fill
    double walletAfter{};     // wallet balance after the fill
    double availableAfter{};  // available balance after the fill
    double positionAfter{};   // signed position after the fill (+ long, - short)
    double marginAfter{};     // initial margin locked after the fill
};

// Simplified USDT-margined BTCUSDT perpetual futures paper account.
//
// THIS IS A PAPER ACCOUNTING MODEL, NOT AN EXACT REPRODUCTION OF BINANCE'S
// PRODUCTION MARGIN ENGINE. Deliberately simplified:
//   - one-way signed position model (positive = long, negative = short)
//   - initial margin = position notional / leverage
//   - no liquidation engine, maintenance-margin tiers, auto-deleveraging,
//     insurance fund, cross/isolated complexity, hedge mode, or live funding.
//
// Reconciliation invariants (always hold):
//   walletBalance      = startingCapital + realizedPnl + fundingPayments
//                        - tradingFees
//   availableBalance   = walletBalance - initialMargin
//   equity(mark)       = walletBalance + unrealizedPnl(mark)
//   netPnl(mark)       = equity(mark) - startingCapital
//                      = realizedPnl + unrealizedPnl(mark) - tradingFees
//                        + fundingPayments
//
// All fills are treated as TAKER (BUY at ask / SELL at bid) and charged the
// configured taker fee rate. The maker rate is stored for a future
// limit-order path and is not applied by the current execution model.
class FuturesAccount
{
public:
    struct Config
    {
        double startingCapital = 10'000.0;
        double leverage = 1.0;
        // Fee placeholders - NOT verified Binance fee tiers. To be researched
        // before any real capital is used.
        double makerFeeRate = 0.0002; // 0.02%
        double takerFeeRate = 0.0004; // 0.04%
        // Simplified funding placeholder: per-interval rate, applied manually.
        double fundingRate = 0.0;
        std::chrono::hours fundingInterval{8};
    };

    FuturesAccount();
    explicit FuturesAccount(Config config);

    // --- Trading (long path used by the current strategy) ---
    // Open or add to a long. Refused if quantity/price is invalid or if the
    // required initial margin + fee exceeds the available balance.
    [[nodiscard]] std::optional<FuturesFill> openLong(double quantity, double price);

    // Close or reduce the long. Refused if quantity/price is invalid or if it
    // exceeds the current position (closeLong never flips into a short).
    [[nodiscard]] std::optional<FuturesFill> closeLong(double quantity, double price);

    // --- Trading (short support: the signed one-way model allows a future
    // SELL-to-open path to be enabled cleanly; not used by the current
    // strategy, which is long-only) ---
    [[nodiscard]] std::optional<FuturesFill> openShort(double quantity, double price);
    [[nodiscard]] std::optional<FuturesFill> closeShort(double quantity, double price);

    // --- Funding (simplified, manual application only; no live funding API) ---
    void setFundingRate(double rate);
    void setFundingInterval(std::chrono::hours interval);
    // Apply the configured funding rate: payment = -position * markPrice * rate.
    // With a positive funding rate the long pays the short, so the payment is
    // negative for longs and positive for shorts. Returns the payment credited
    // to the account (negative = paid out).
    [[nodiscard]] double applyFunding(double markPrice);
    [[nodiscard]] double applyFunding(double markPrice, double rate);

    // --- Configuration ---
    [[nodiscard]] double startingCapital() const;
    [[nodiscard]] double leverage() const;
    void setLeverage(double leverage);
    [[nodiscard]] double makerFeeRate() const;
    [[nodiscard]] double takerFeeRate() const;
    [[nodiscard]] double fundingRate() const;
    [[nodiscard]] std::chrono::hours fundingInterval() const;

    // --- Account state ---
    [[nodiscard]] double walletBalance() const;
    [[nodiscard]] double availableBalance() const;
    [[nodiscard]] double equity(double markPrice) const;
    [[nodiscard]] double position() const;           // signed BTC (+ long, - short)
    [[nodiscard]] double averageEntryPrice() const;
    [[nodiscard]] double positionNotional() const;   // |position| * avgEntry
    [[nodiscard]] double initialMargin() const;      // positionNotional / leverage

    // --- P&L components (kept separate by construction) ---
    [[nodiscard]] double realizedPnl() const;        // gross, price-based
    [[nodiscard]] double unrealizedPnl(double markPrice) const;
    [[nodiscard]] double tradingFees() const;
    [[nodiscard]] double fundingPayments() const;    // net credited (negative = paid)
    [[nodiscard]] double netPnl(double markPrice) const;

private:
    [[nodiscard]] FuturesFill makeFill(double price, double quantity, double fee) const;

    Config config_;
    double position_{0.0};
    double averageEntryPrice_{0.0};
    double realizedPnl_{0.0};
    double tradingFees_{0.0};
    double fundingPayments_{0.0};
};

} // namespace nkm
