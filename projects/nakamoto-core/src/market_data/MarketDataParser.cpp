#include "market_data/MarketDataParser.hpp"

#include <optional>
#include <string_view>

namespace nkm {

std::optional<DepthEvent> MarketDataParser::parseDepthEvent(std::string_view json)
{
    try {
        const simdjson::padded_string paddedJson(json);
        auto doc = parser_.iterate(paddedJson);
        if (doc.error()) {
            return std::nullopt;
        }

        auto root = doc.get_object();
        if (root.error()) {
            return std::nullopt;
        }

        auto eventNameResult = root["e"].get_string();
        if (eventNameResult.error()) {
            return std::nullopt;
        }

        const std::string_view eventName = eventNameResult.value_unsafe();
        if (eventName != "depthUpdate") {
            return std::nullopt;
        }

        DepthEvent depthEvent;

        const auto timestampResult = root["E"].get_uint64();
        if (timestampResult.error()) {
            return std::nullopt;
        }
        depthEvent.eventTimestamp = timestampResult.value_unsafe();

        const auto symbolResult = root["s"].get_string();
        if (symbolResult.error()) {
            return std::nullopt;
        }
        const std::string_view symbol = symbolResult.value_unsafe();
        depthEvent.symbol.assign(symbol.data(), symbol.size());

        const auto firstUpdateIdResult = root["U"].get_uint64();
        if (firstUpdateIdResult.error()) {
            return std::nullopt;
        }
        depthEvent.firstUpdateId = firstUpdateIdResult.value_unsafe();

        const auto finalUpdateIdResult = root["u"].get_uint64();
        if (finalUpdateIdResult.error()) {
            return std::nullopt;
        }
        depthEvent.finalUpdateId = finalUpdateIdResult.value_unsafe();

        const auto previousFinalUpdateIdResult = root["pu"].get_uint64();
        if (previousFinalUpdateIdResult.error()) {
            return std::nullopt;
        }
        depthEvent.previousFinalUpdateId = previousFinalUpdateIdResult.value_unsafe();

        const auto bidArrayResult = root["b"].get_array();
        if (bidArrayResult.error()) {
            return std::nullopt;
        }

        auto bidArray = bidArrayResult.value_unsafe();
        for (auto bidEntry : bidArray) {
            auto bidEntryValue = bidEntry.value_unsafe();
            auto bidLevelsResult = bidEntryValue.get_array();
            if (bidLevelsResult.error()) {
                return std::nullopt;
            }

            auto bidLevels = bidLevelsResult.value_unsafe();
            auto bidIterator = bidLevels.begin();
            if (bidIterator.error()) {
                return std::nullopt;
            }
            if (bidIterator == bidLevels.end()) {
                return std::nullopt;
            }

            auto priceValue = *bidIterator;
            auto priceResult = priceValue.get_double_in_string();
            if (priceResult.error()) {
                return std::nullopt;
            }
            const double price = priceResult.value_unsafe();

            ++bidIterator;
            if (bidIterator == bidLevels.end()) {
                return std::nullopt;
            }

            auto quantityValue = *bidIterator;
            auto quantityResult = quantityValue.get_double_in_string();
            if (quantityResult.error()) {
                return std::nullopt;
            }
            const double quantity = quantityResult.value_unsafe();

            depthEvent.bids.push_back(PriceLevel{price, quantity});
        }

        const auto askArrayResult = root["a"].get_array();
        if (askArrayResult.error()) {
            return std::nullopt;
        }

        auto askArray = askArrayResult.value_unsafe();
        for (auto askEntry : askArray) {
            auto askEntryValue = askEntry.value_unsafe();
            auto askLevelsResult = askEntryValue.get_array();
            if (askLevelsResult.error()) {
                return std::nullopt;
            }

            auto askLevels = askLevelsResult.value_unsafe();
            auto askIterator = askLevels.begin();
            if (askIterator.error()) {
                return std::nullopt;
            }
            if (askIterator == askLevels.end()) {
                return std::nullopt;
            }

            auto priceValue = *askIterator;
            auto priceResult = priceValue.get_double_in_string();
            if (priceResult.error()) {
                return std::nullopt;
            }
            const double price = priceResult.value_unsafe();

            ++askIterator;
            if (askIterator == askLevels.end()) {
                return std::nullopt;
            }

            auto quantityValue = *askIterator;
            auto quantityResult = quantityValue.get_double_in_string();
            if (quantityResult.error()) {
                return std::nullopt;
            }
            const double quantity = quantityResult.value_unsafe();

            depthEvent.asks.push_back(PriceLevel{price, quantity});
        }

        return depthEvent;
    }
    catch (const simdjson::simdjson_error&) {
        return std::nullopt;
    }
}

} // namespace nkm
