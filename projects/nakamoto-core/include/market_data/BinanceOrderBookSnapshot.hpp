#pragma once

#include "market_data/DepthEvent.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace nkm {

// Initial REST snapshot of the Binance USDⓈ-M Futures order book.
struct OrderBookSnapshot
{
    std::uint64_t lastUpdateId{};
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

// Acquires the initial order-book snapshot via the Binance Futures REST
// endpoint GET /fapi/v1/depth. Kept separate from the WebSocket client.
class BinanceOrderBookSnapshot
{
public:
    [[nodiscard]] std::optional<OrderBookSnapshot> fetch(std::string_view symbol) const;
};

} // namespace nkm
