#pragma once

#include "market_data/DepthEvent.hpp"

#include <functional>
#include <memory>
#include <string_view>

namespace nkm {

class BinanceFuturesClient
{
public:
    using DepthEventSink = std::function<void(const DepthEvent&)>;

    BinanceFuturesClient();
    ~BinanceFuturesClient();

    BinanceFuturesClient(const BinanceFuturesClient&) = delete;
    BinanceFuturesClient& operator=(const BinanceFuturesClient&) = delete;

    BinanceFuturesClient(BinanceFuturesClient&&) noexcept;
    BinanceFuturesClient& operator=(BinanceFuturesClient&&) noexcept;

    [[nodiscard]] bool connect();
    [[nodiscard]] bool subscribeDepth(std::string_view symbol);
    [[nodiscard]] bool subscribeTrades(std::string_view symbol);
    void run(const DepthEventSink& sink);
    void disconnect();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nkm
