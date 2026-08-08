#include "trading/PaperExecutionEngine.hpp"

namespace nkm {

namespace {

constexpr double kTolerance = 1e-9;

} // namespace

PaperExecutionEngine::PaperExecutionEngine(double startingCash,
                                           double feeRate,
                                           double maxPositionSize,
                                           double tradeQuantity)
    : startingCash_(startingCash)
    , cash_(startingCash)
    , feeRate_(feeRate)
    , maxPositionSize_(maxPositionSize)
    , tradeQuantity_(tradeQuantity)
{
}

std::optional<PaperTrade> PaperExecutionEngine::tryBuy(double price, double imbalance)
{
    // Risk limits: refuse trades that would exceed the maximum position size,
    // and refuse to buy beyond available cash (no leverage logic yet).
    if (position_ + tradeQuantity_ > maxPositionSize_ + kTolerance) {
        return std::nullopt;
    }

    const double notional = tradeQuantity_ * price;
    const double fee = notional * feeRate_;
    if (cash_ < notional + fee) {
        return std::nullopt;
    }

    // Average-cost entry update.
    averageEntryPrice_ =
        (averageEntryPrice_ * position_ + notional) / (position_ + tradeQuantity_);

    position_ += tradeQuantity_;
    cash_ -= notional + fee;
    feesPaid_ += fee;
    ++tradeCount_;

    return PaperTrade{Signal::Buy,
                      price,
                      tradeQuantity_,
                      fee,
                      imbalance,
                      cash_,
                      position_};
}

std::optional<PaperTrade> PaperExecutionEngine::trySell(double price, double imbalance)
{
    // No short selling in this baseline: cannot sell more than the position.
    if (tradeQuantity_ > position_ + kTolerance) {
        return std::nullopt;
    }

    const double notional = tradeQuantity_ * price;
    const double fee = notional * feeRate_;

    realizedPnl_ += (price - averageEntryPrice_) * tradeQuantity_;

    position_ -= tradeQuantity_;
    cash_ += notional - fee;
    feesPaid_ += fee;
    ++tradeCount_;

    if (position_ <= kTolerance) {
        position_ = 0.0;
        averageEntryPrice_ = 0.0;
    }

    return PaperTrade{Signal::Sell,
                      price,
                      tradeQuantity_,
                      fee,
                      imbalance,
                      cash_,
                      position_};
}

double PaperExecutionEngine::cash() const
{
    return cash_;
}

double PaperExecutionEngine::position() const
{
    return position_;
}

double PaperExecutionEngine::averageEntryPrice() const
{
    return averageEntryPrice_;
}

double PaperExecutionEngine::realizedPnl() const
{
    return realizedPnl_;
}

double PaperExecutionEngine::unrealizedPnl(double markPrice) const
{
    return (markPrice - averageEntryPrice_) * position_;
}

double PaperExecutionEngine::totalPnl(double markPrice) const
{
    return realizedPnl_ + unrealizedPnl(markPrice) - feesPaid_;
}

double PaperExecutionEngine::feesPaid() const
{
    return feesPaid_;
}

std::size_t PaperExecutionEngine::tradeCount() const
{
    return tradeCount_;
}

double PaperExecutionEngine::startingCash() const
{
    return startingCash_;
}

double PaperExecutionEngine::feeRate() const
{
    return feeRate_;
}

double PaperExecutionEngine::maxPositionSize() const
{
    return maxPositionSize_;
}

double PaperExecutionEngine::tradeQuantity() const
{
    return tradeQuantity_;
}

double PaperExecutionEngine::equity(double markPrice) const
{
    return cash_ + position_ * markPrice;
}

} // namespace nkm
