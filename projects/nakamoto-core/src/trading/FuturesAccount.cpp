#include "trading/FuturesAccount.hpp"

#include <cmath>

namespace nkm {

namespace {

constexpr double kTolerance = 1e-9;

} // namespace

FuturesAccount::FuturesAccount()
    : FuturesAccount(Config{})
{
}

FuturesAccount::FuturesAccount(Config config)
    : config_(config)
{
    // Sanitize obvious misconfiguration so the invariants stay well-defined.
    if (config_.leverage <= 0.0) config_.leverage = 1.0;
    if (config_.startingCapital < 0.0) config_.startingCapital = 0.0;
    if (config_.makerFeeRate < 0.0) config_.makerFeeRate = 0.0;
    if (config_.takerFeeRate < 0.0) config_.takerFeeRate = 0.0;
    if (config_.fundingInterval.count() <= 0) config_.fundingInterval = std::chrono::hours{8};
}

FuturesFill FuturesAccount::makeFill(double price, double quantity, double fee) const
{
    return FuturesFill{price,
                       quantity,
                       fee,
                       walletBalance(),
                       availableBalance(),
                       position_,
                       initialMargin()};
}

std::optional<FuturesFill> FuturesAccount::openLong(double quantity, double price)
{
    if (quantity <= 0.0 || price <= 0.0 || position_ < 0.0) {
        return std::nullopt;
    }

    const double fee = quantity * price * config_.takerFeeRate;
    const double requiredMargin = quantity * price / config_.leverage;

    // Simplified margin check: required margin + fee must fit the available
    // balance (wallet minus margin already locked by an open position).
    if (requiredMargin + fee > availableBalance() + kTolerance) {
        return std::nullopt;
    }

    // Weighted-average entry price across the accumulated position.
    averageEntryPrice_ =
        (averageEntryPrice_ * position_ + quantity * price) / (position_ + quantity);
    position_ += quantity;
    tradingFees_ += fee;

    return makeFill(price, quantity, fee);
}

std::optional<FuturesFill> FuturesAccount::closeLong(double quantity, double price)
{
    if (quantity <= 0.0 || price <= 0.0 || position_ <= 0.0 ||
        quantity > position_ + kTolerance) {
        return std::nullopt;
    }

    const double fee = quantity * price * config_.takerFeeRate;

    // Realize P&L on the closed quantity; the remainder keeps the average
    // entry price (partial close).
    realizedPnl_ += (price - averageEntryPrice_) * quantity;
    tradingFees_ += fee;
    position_ -= quantity;

    if (position_ <= kTolerance) {
        position_ = 0.0;
        averageEntryPrice_ = 0.0;
    }

    return makeFill(price, quantity, fee);
}

std::optional<FuturesFill> FuturesAccount::openShort(double quantity, double price)
{
    if (quantity <= 0.0 || price <= 0.0 || position_ > 0.0) {
        return std::nullopt;
    }

    const double fee = quantity * price * config_.takerFeeRate;
    const double requiredMargin = quantity * price / config_.leverage;
    if (requiredMargin + fee > availableBalance() + kTolerance) {
        return std::nullopt;
    }

    const double existing = -position_; // absolute size of the current short
    averageEntryPrice_ =
        (averageEntryPrice_ * existing + quantity * price) / (existing + quantity);
    position_ -= quantity;
    tradingFees_ += fee;

    return makeFill(price, quantity, fee);
}

std::optional<FuturesFill> FuturesAccount::closeShort(double quantity, double price)
{
    if (quantity <= 0.0 || price <= 0.0 || position_ >= 0.0 ||
        quantity > -position_ + kTolerance) {
        return std::nullopt;
    }

    const double fee = quantity * price * config_.takerFeeRate;

    realizedPnl_ += (averageEntryPrice_ - price) * quantity;
    tradingFees_ += fee;
    position_ += quantity;

    if (position_ >= -kTolerance) {
        position_ = 0.0;
        averageEntryPrice_ = 0.0;
    }

    return makeFill(price, quantity, fee);
}

void FuturesAccount::setFundingRate(double rate)
{
    config_.fundingRate = rate;
}

void FuturesAccount::setFundingInterval(std::chrono::hours interval)
{
    config_.fundingInterval = interval.count() > 0 ? interval : std::chrono::hours{8};
}

double FuturesAccount::applyFunding(double markPrice, double rate)
{
    const double payment = -position_ * markPrice * rate;
    fundingPayments_ += payment;
    return payment;
}

double FuturesAccount::applyFunding(double markPrice)
{
    return applyFunding(markPrice, config_.fundingRate);
}

double FuturesAccount::startingCapital() const
{
    return config_.startingCapital;
}

double FuturesAccount::leverage() const
{
    return config_.leverage;
}

void FuturesAccount::setLeverage(double leverage)
{
    config_.leverage = leverage > 0.0 ? leverage : 1.0;
}

double FuturesAccount::makerFeeRate() const
{
    return config_.makerFeeRate;
}

double FuturesAccount::takerFeeRate() const
{
    return config_.takerFeeRate;
}

double FuturesAccount::fundingRate() const
{
    return config_.fundingRate;
}

std::chrono::hours FuturesAccount::fundingInterval() const
{
    return config_.fundingInterval;
}

double FuturesAccount::walletBalance() const
{
    return config_.startingCapital + realizedPnl_ + fundingPayments_ - tradingFees_;
}

double FuturesAccount::availableBalance() const
{
    return walletBalance() - initialMargin();
}

double FuturesAccount::equity(double markPrice) const
{
    return walletBalance() + unrealizedPnl(markPrice);
}

double FuturesAccount::position() const
{
    return position_;
}

double FuturesAccount::averageEntryPrice() const
{
    return averageEntryPrice_;
}

double FuturesAccount::positionNotional() const
{
    return std::fabs(position_) * averageEntryPrice_;
}

double FuturesAccount::initialMargin() const
{
    return positionNotional() / config_.leverage;
}

double FuturesAccount::realizedPnl() const
{
    return realizedPnl_;
}

double FuturesAccount::unrealizedPnl(double markPrice) const
{
    // One formula works for both signs of the position:
    //   long:  (mark - entry) * qty
    //   short: (entry - mark) * |qty| == (mark - entry) * qty
    return (markPrice - averageEntryPrice_) * position_;
}

double FuturesAccount::tradingFees() const
{
    return tradingFees_;
}

double FuturesAccount::fundingPayments() const
{
    return fundingPayments_;
}

double FuturesAccount::netPnl(double markPrice) const
{
    return realizedPnl_ + unrealizedPnl(markPrice) - tradingFees_ + fundingPayments_;
}

} // namespace nkm
