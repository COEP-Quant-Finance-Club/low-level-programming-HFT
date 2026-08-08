#pragma once

#include "trading/Strategy.hpp"

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
    double cashAfter{};
    double positionAfter{};
};

// Single-threaded paper trading account.
//
// Assumptions (all configurable):
//   - BUY  fills at the current best ask, SELL fills at the current best bid
//     (the caller supplies the price from the synchronized order book).
//   - fee_rate is a flat proportional fee on notional per side; the default is
//     a conservative placeholder (0.02%) and must NOT be claimed as a real
//     Binance fee tier.
//   - max_position_size caps the absolute position; trades that would exceed
//     it are refused.
//   - A BUY is refused when there is not enough cash to cover notional + fee
//     (no leverage logic yet).
//   - Short selling is disabled: a SELL with insufficient position is refused.
//   - Each signal trades a fixed, configurable quantity.
//
// Accounting:
//   - realized P&L is the gross price-based P&L from round trips
//     (sell_price - avg_entry) * quantity; fees are tracked separately.
//   - unrealized P&L is (mark_price - avg_entry) * position.
//   - total P&L is net of fees: realized + unrealized - feesPaid, so it
//     reconciles with equity(markPrice) - startingCash.
class PaperExecutionEngine
{
public:
    PaperExecutionEngine(double startingCash,
                         double feeRate,
                         double maxPositionSize,
                         double tradeQuantity);

    // Attempt a simulated buy at `price`. Returns the trade on success, or
    // std::nullopt if it would exceed max_position_size or cash is insufficient.
    [[nodiscard]] std::optional<PaperTrade> tryBuy(double price, double imbalance);

    // Attempt a simulated sell at `price`. Returns the trade on success, or
    // std::nullopt if there is not enough position (no shorting).
    [[nodiscard]] std::optional<PaperTrade> trySell(double price, double imbalance);

    [[nodiscard]] double cash() const;
    [[nodiscard]] double position() const;
    [[nodiscard]] double averageEntryPrice() const;
    [[nodiscard]] double realizedPnl() const;
    [[nodiscard]] double unrealizedPnl(double markPrice) const;

    // Net P&L: realized + unrealized - feesPaid.
    [[nodiscard]] double totalPnl(double markPrice) const;
    [[nodiscard]] double feesPaid() const;
    [[nodiscard]] std::size_t tradeCount() const;

    [[nodiscard]] double startingCash() const;
    [[nodiscard]] double feeRate() const;
    [[nodiscard]] double maxPositionSize() const;
    [[nodiscard]] double tradeQuantity() const;

    // Account equity mark-to-market: cash + position * markPrice.
    [[nodiscard]] double equity(double markPrice) const;

private:
    double startingCash_;
    double cash_;
    double position_{0.0};
    double averageEntryPrice_{0.0};
    double realizedPnl_{0.0};
    double feesPaid_{0.0};
    std::size_t tradeCount_{0};

    double feeRate_;
    double maxPositionSize_;
    double tradeQuantity_;
};

} // namespace nkm
