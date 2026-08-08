#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nkm {

struct PriceLevel
{
    double price{};
    double quantity{};
};

struct DepthEvent
{
    std::uint64_t eventTimestamp{};
    std::string symbol;
    std::uint64_t firstUpdateId{};        // U - first update ID covered by this event
    std::uint64_t finalUpdateId{};        // u - final update ID covered by this event
    std::uint64_t previousFinalUpdateId{}; // pu - final update ID of the previous event
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

} // namespace nkm
