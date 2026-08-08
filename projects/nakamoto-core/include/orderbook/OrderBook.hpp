#pragma once

#include "market_data/DepthEvent.hpp"

#include <cstddef>
#include <map>

namespace nkm {

class OrderBook
{
public:
    OrderBook() = default;
    ~OrderBook() = default;

    OrderBook(const OrderBook&) = default;
    OrderBook& operator=(const OrderBook&) = default;
    OrderBook(OrderBook&&) noexcept = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;

    void applyDepthUpdate(const DepthEvent& event);
    void applySnapshot(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);

    [[nodiscard]] std::size_t bidLevelCount() const;
    [[nodiscard]] std::size_t askLevelCount() const;

    [[nodiscard]] double bestBid() const;
    [[nodiscard]] double bestAsk() const;
    [[nodiscard]] double spread() const;
    [[nodiscard]] double midPrice() const;

    [[nodiscard]] double bestBidQuantity(double price) const;
    [[nodiscard]] double bestAskQuantity(double price) const;
    [[nodiscard]] double bidQuantity(double price) const;
    [[nodiscard]] double askQuantity(double price) const;

    // Visit the top `n` bid levels (highest price first) as (price, quantity).
    template <typename Fn>
    void forEachTopBid(std::size_t n, Fn&& fn) const
    {
        std::size_t i = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && i < n; ++it, ++i) {
            fn(it->first, it->second);
        }
    }

    // Visit the top `n` ask levels (lowest price first) as (price, quantity).
    template <typename Fn>
    void forEachTopAsk(std::size_t n, Fn&& fn) const
    {
        std::size_t i = 0;
        for (auto it = asks_.begin(); it != asks_.end() && i < n; ++it, ++i) {
            fn(it->first, it->second);
        }
    }

private:
    void applyBidUpdate(const PriceLevel& level);
    void applyAskUpdate(const PriceLevel& level);

    std::map<double, double> bids_;
    std::map<double, double> asks_;
};

} // namespace nkm
