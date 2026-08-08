#include "trading/OrderBookImbalance.hpp"

namespace nkm {

OrderBookImbalance::OrderBookImbalance(std::size_t topLevels)
    : topLevels_(topLevels)
{
}

double OrderBookImbalance::calculate(const OrderBook& book) const
{
    double bidVolume = 0.0;
    double askVolume = 0.0;

    book.forEachTopBid(topLevels_, [&bidVolume](double /*price*/, double quantity) {
        bidVolume += quantity;
    });
    book.forEachTopAsk(topLevels_, [&askVolume](double /*price*/, double quantity) {
        askVolume += quantity;
    });

    const double totalVolume = bidVolume + askVolume;
    if (totalVolume <= 0.0) {
        // Zero-volume book: no signal.
        return 0.0;
    }

    return (bidVolume - askVolume) / totalVolume;
}

std::size_t OrderBookImbalance::topLevels() const
{
    return topLevels_;
}

} // namespace nkm
