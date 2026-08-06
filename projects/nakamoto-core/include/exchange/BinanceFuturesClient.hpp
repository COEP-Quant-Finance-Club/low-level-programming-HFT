#pragma once

#include <memory>
#include <string_view>

namespace nkm {

class BinanceFuturesClient
{
public:
    BinanceFuturesClient();
    ~BinanceFuturesClient();

    BinanceFuturesClient(const BinanceFuturesClient&) = delete;
    BinanceFuturesClient& operator=(const BinanceFuturesClient&) = delete;

    BinanceFuturesClient(BinanceFuturesClient&&) noexcept;
    BinanceFuturesClient& operator=(BinanceFuturesClient&&) noexcept;

    [[nodiscard]] bool connect();
    [[nodiscard]] bool subscribeTrades(std::string_view symbol);
    void run();
    void disconnect();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nkm
