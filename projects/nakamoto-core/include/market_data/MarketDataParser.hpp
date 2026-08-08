#pragma once

#include "market_data/DepthEvent.hpp"

#include <simdjson.h>

#include <optional>
#include <string_view>

namespace nkm {

class MarketDataParser
{
public:
    [[nodiscard]] std::optional<DepthEvent> parseDepthEvent(std::string_view json);

private:
    simdjson::ondemand::parser parser_;
};

} // namespace nkm
