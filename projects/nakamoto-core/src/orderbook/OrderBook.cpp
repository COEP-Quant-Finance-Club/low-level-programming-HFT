#include "orderbook/OrderBook.hpp"

#include <algorithm>
#include <limits>

namespace nkm {

void OrderBook::applyDepthUpdate(const DepthEvent& event)
{
    for (const auto& bid : event.bids) {
        applyBidUpdate(bid);
    }

    for (const auto& ask : event.asks) {
        applyAskUpdate(ask);
    }
}

void OrderBook::applySnapshot(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks)
{
    bids_.clear();
    asks_.clear();

    for (const auto& bid : bids) {
        applyBidUpdate(bid);
    }

    for (const auto& ask : asks) {
        applyAskUpdate(ask);
    }
}

std::size_t OrderBook::bidLevelCount() const
{
    return bids_.size();
}

std::size_t OrderBook::askLevelCount() const
{
    return asks_.size();
}

void OrderBook::applyBidUpdate(const PriceLevel& level)
{
    if (level.quantity == 0.0) {
        bids_.erase(level.price);
        return;
    }

    bids_[level.price] = level.quantity;
}

void OrderBook::applyAskUpdate(const PriceLevel& level)
{
    if (level.quantity == 0.0) {
        asks_.erase(level.price);
        return;
    }

    asks_[level.price] = level.quantity;
}

double OrderBook::bestBid() const
{
    if (bids_.empty()) {
        return 0.0;
    }

    return bids_.rbegin()->first;
}

double OrderBook::bestAsk() const
{
    if (asks_.empty()) {
        return 0.0;
    }

    return asks_.begin()->first;
}

double OrderBook::spread() const
{
    const double bid = bestBid();
    const double ask = bestAsk();

    if (bid == 0.0 || ask == 0.0) {
        return 0.0;
    }

    return ask - bid;
}

double OrderBook::midPrice() const
{
    const double bid = bestBid();
    const double ask = bestAsk();

    if (bid == 0.0 || ask == 0.0) {
        return 0.0;
    }

    return (bid + ask) / 2.0;
}

double OrderBook::bestBidQuantity(double price) const
{
    return bidQuantity(price);
}

double OrderBook::bestAskQuantity(double price) const
{
    return askQuantity(price);
}

double OrderBook::bidQuantity(double price) const
{
    const auto it = bids_.find(price);
    if (it == bids_.end()) {
        return 0.0;
    }

    return it->second;
}

double OrderBook::askQuantity(double price) const
{
    const auto it = asks_.find(price);
    if (it == asks_.end()) {
        return 0.0;
    }

    return it->second;
}

} // namespace nkm
