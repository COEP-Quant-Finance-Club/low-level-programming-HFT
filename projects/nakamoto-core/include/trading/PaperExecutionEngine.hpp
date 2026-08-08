#pragma once

#include "trading/FuturesAccount.hpp"
#include "trading/Strategy.hpp"

#include <chrono>
#include <cstddef>
#include <optional>

namespace nkm {

// Result of a simulated fill. The engine never contacts an exchange.
struct PaperTrade
{
    Signal side{Signal::Hold};
    double price{};
    double quantity{};
    double fee{};
    double imbalance{};
    double walletAfter{};
    double availableAfter{};
    double positionAfter{};  // signed BTC (+ long, - short)
    double marginAfter{};
};

// Single-threaded paper trading engine for a simplified USDT-margined
// perpetual futures account (accounting lives in FuturesAccount).
//
// Execution assumptions (all configurable):
//   - BUY fills at the current best ask, SELL fills at the current best bid
//     (the caller supplies the price from the synchronized order book), so
//     fills are treated as TAKER and charged the taker fee rate.
//   - One-way position model. The current strategy is long-only: a SELL only
//     closes/reduces an existing long and is refused with no position. Shorts
//     are supported by the underlying FuturesAccount but not opened here.
//   - max_position_size caps the absolute position; trades that would exceed
//     it are refused.
//   - A BUY is refused when the required initial margin + fee exceeds the
//     available balance (simplified margin model: margin = notional / leverage).
//   - Each signal trades a fixed, configurable quantity.
//
// Accounting model (see FuturesAccount):
//   - wallet balance, available balance and margin are distinct; opening a
//     position locks margin instead of debiting the full notional.
//   - gross realized P&L, unrealized P&L, trading fees and funding payments
//     are tracked separately and only combined into net P&L at display time.
class PaperExecutionEngine
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
        // Maximum absolute position in base units (BTC).
        double maxPositionSize = 1.0;
        // Fixed base quantity per paper trade.
        double tradeQuantity = 0.05;
    };

    PaperExecutionEngine();
    explicit PaperExecutionEngine(Config config);

    // BUY-to-open/add long at `price`. Returns the trade on success, or
    // std::nullopt if it would exceed max_position_size or the required margin
    // + fee exceeds the available balance.
    [[nodiscard]] std::optional<PaperTrade> tryBuy(double price, double imbalance);

    // SELL-to-close/reduce long at `price`. Returns the trade on success, or
    // std::nullopt if there is not enough position (no shorts in the engine).
    [[nodiscard]] std::optional<PaperTrade> trySell(double price, double imbalance);

    // --- Account state (forwarded from FuturesAccount) ---
    [[nodiscard]] double walletBalance() const;
    [[nodiscard]] double availableBalance() const;
    [[nodiscard]] double equity(double markPrice) const;
    [[nodiscard]] double position() const;           // signed BTC
    [[nodiscard]] double averageEntryPrice() const;
    [[nodiscard]] double positionNotional() const;
    [[nodiscard]] double initialMargin() const;
    [[nodiscard]] double unrealizedPnl(double markPrice) const;
    [[nodiscard]] double realizedPnl() const;
    [[nodiscard]] double tradingFees() const;
    [[nodiscard]] double fundingPayments() const;
    [[nodiscard]] double netPnl(double markPrice) const;
    [[nodiscard]] double startingCapital() const;
    [[nodiscard]] std::size_t tradeCount() const;

    // --- Funding: simplified manual application (no live funding API) ---
    void setFundingRate(double rate);
    [[nodiscard]] double applyFunding(double markPrice);
    [[nodiscard]] double applyFunding(double markPrice, double rate);

    // --- Configuration accessors ---
    [[nodiscard]] double leverage() const;
    [[nodiscard]] double makerFeeRate() const;
    [[nodiscard]] double takerFeeRate() const;
    [[nodiscard]] double fundingRate() const;
    [[nodiscard]] double maxPositionSize() const;
    [[nodiscard]] double tradeQuantity() const;

private:
    FuturesAccount account_;
    double maxPositionSize_;
    double tradeQuantity_;
    std::size_t tradeCount_{0};
};

} // namespace nkm
