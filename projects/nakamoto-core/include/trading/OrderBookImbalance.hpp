#pragma once

#include "orderbook/OrderBook.hpp"

#include <cstddef>

namespace nkm {

// Market-state feature: measures the supply/demand imbalance across the top N
// price levels of the order book.
//
//   imbalance = (bid_volume - ask_volume) / (bid_volume + ask_volume)
//
// Result is in [-1, +1]: +1 extremely bid-heavy, 0 balanced, -1 extremely
// ask-heavy. Zero-volume books return 0.0.
//
// Kept separate from the OrderBook itself.
class OrderBookImbalance
{
public:
    explicit OrderBookImbalance(std::size_t topLevels = 5);

    [[nodiscard]] double calculate(const OrderBook& book) const;

    [[nodiscard]] std::size_t topLevels() const;

private:
    std::size_t topLevels_;
};

} // namespace nkm
