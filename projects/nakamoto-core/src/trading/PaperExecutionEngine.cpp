#include "trading/PaperExecutionEngine.hpp"

namespace nkm {

namespace {

constexpr double kTolerance = 1e-9;

} // namespace

PaperExecutionEngine::PaperExecutionEngine()
    : PaperExecutionEngine(Config{})
{
}

PaperExecutionEngine::PaperExecutionEngine(Config config)
    : account_(FuturesAccount::Config{config.startingCapital,
                                      config.leverage,
                                      config.makerFeeRate,
                                      config.takerFeeRate,
                                      config.fundingRate,
                                      config.fundingInterval})
    , maxPositionSize_(config.maxPositionSize)
    , tradeQuantity_(config.tradeQuantity)
{
}

std::optional<PaperTrade> PaperExecutionEngine::tryBuy(double price, double imbalance)
{
    if (price <= 0.0) {
        return std::nullopt;
    }

    // Risk limit: never exceed the configured maximum absolute position.
    if (position() + tradeQuantity_ > maxPositionSize_ + kTolerance) {
        return std::nullopt;
    }

    // The account refuses the order when required margin + fee would exceed
    // the available balance (simplified margin model).
    const auto fill = account_.openLong(tradeQuantity_, price);
    if (!fill) {
        return std::nullopt;
    }

    ++tradeCount_;
    return PaperTrade{Signal::Buy,
                      fill->price,
                      fill->quantity,
                      fill->fee,
                      imbalance,
                      fill->walletAfter,
                      fill->availableAfter,
                      fill->positionAfter,
                      fill->marginAfter};
}

std::optional<PaperTrade> PaperExecutionEngine::trySell(double price, double imbalance)
{
    if (price <= 0.0) {
        return std::nullopt;
    }

    // The current strategy is long-only: a SELL only closes an existing long.
    // SELL-to-open (shorts) is left for a future sprint.
    if (tradeQuantity_ > position() + kTolerance) {
        return std::nullopt;
    }

    const auto fill = account_.closeLong(tradeQuantity_, price);
    if (!fill) {
        return std::nullopt;
    }

    ++tradeCount_;
    return PaperTrade{Signal::Sell,
                      fill->price,
                      fill->quantity,
                      fill->fee,
                      imbalance,
                      fill->walletAfter,
                      fill->availableAfter,
                      fill->positionAfter,
                      fill->marginAfter};
}

double PaperExecutionEngine::walletBalance() const
{
    return account_.walletBalance();
}

double PaperExecutionEngine::availableBalance() const
{
    return account_.availableBalance();
}

double PaperExecutionEngine::equity(double markPrice) const
{
    return account_.equity(markPrice);
}

double PaperExecutionEngine::position() const
{
    return account_.position();
}

double PaperExecutionEngine::averageEntryPrice() const
{
    return account_.averageEntryPrice();
}

double PaperExecutionEngine::positionNotional() const
{
    return account_.positionNotional();
}

double PaperExecutionEngine::initialMargin() const
{
    return account_.initialMargin();
}

double PaperExecutionEngine::unrealizedPnl(double markPrice) const
{
    return account_.unrealizedPnl(markPrice);
}

double PaperExecutionEngine::realizedPnl() const
{
    return account_.realizedPnl();
}

double PaperExecutionEngine::tradingFees() const
{
    return account_.tradingFees();
}

double PaperExecutionEngine::fundingPayments() const
{
    return account_.fundingPayments();
}

double PaperExecutionEngine::netPnl(double markPrice) const
{
    return account_.netPnl(markPrice);
}

double PaperExecutionEngine::startingCapital() const
{
    return account_.startingCapital();
}

std::size_t PaperExecutionEngine::tradeCount() const
{
    return tradeCount_;
}

void PaperExecutionEngine::setFundingRate(double rate)
{
    account_.setFundingRate(rate);
}

double PaperExecutionEngine::applyFunding(double markPrice)
{
    return account_.applyFunding(markPrice);
}

double PaperExecutionEngine::applyFunding(double markPrice, double rate)
{
    return account_.applyFunding(markPrice, rate);
}

double PaperExecutionEngine::leverage() const
{
    return account_.leverage();
}

double PaperExecutionEngine::makerFeeRate() const
{
    return account_.makerFeeRate();
}

double PaperExecutionEngine::takerFeeRate() const
{
    return account_.takerFeeRate();
}

double PaperExecutionEngine::fundingRate() const
{
    return account_.fundingRate();
}

double PaperExecutionEngine::maxPositionSize() const
{
    return maxPositionSize_;
}

double PaperExecutionEngine::tradeQuantity() const
{
    return tradeQuantity_;
}

} // namespace nkm
