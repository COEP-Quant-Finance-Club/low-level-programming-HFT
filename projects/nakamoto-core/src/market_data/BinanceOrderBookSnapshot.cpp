#include "market_data/BinanceOrderBookSnapshot.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <simdjson.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace nkm {

namespace {

constexpr char kBinanceFuturesTestnetHost[] = "testnet.binancefuture.com";
constexpr char kBinanceFuturesTestnetPort[] = "443";
constexpr std::uint64_t kDepthLimit = 1000;

// Parses a Binance [[price, quantity], ...] array into PriceLevel objects.
bool parseLevelArray(simdjson::ondemand::array& array, std::vector<PriceLevel>& out)
{
    for (auto entry : array) {
        auto levelArrayResult = entry.value_unsafe().get_array();
        if (levelArrayResult.error()) {
            return false;
        }

        auto levelArray = levelArrayResult.value_unsafe();
        auto levelIterator = levelArray.begin();
        if (levelIterator.error() || levelIterator == levelArray.end()) {
            return false;
        }

        auto priceResult = (*levelIterator).get_double_in_string();
        if (priceResult.error()) {
            return false;
        }
        const double price = priceResult.value_unsafe();

        ++levelIterator;
        if (levelIterator == levelArray.end()) {
            return false;
        }

        auto quantityResult = (*levelIterator).get_double_in_string();
        if (quantityResult.error()) {
            return false;
        }
        const double quantity = quantityResult.value_unsafe();

        // Zero-quantity levels in a snapshot carry no information; skip them.
        if (quantity > 0.0) {
            out.push_back(PriceLevel{price, quantity});
        }
    }
    return true;
}

} // namespace

std::optional<OrderBookSnapshot> BinanceOrderBookSnapshot::fetch(std::string_view symbol) const
{
    try {
        net::io_context io;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        tcp::resolver resolver(io);
        ssl::stream<beast::tcp_stream> stream(io, ctx);
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(10));

        if (!SSL_set_tlsext_host_name(stream.native_handle(), kBinanceFuturesTestnetHost)) {
            return std::nullopt;
        }

        const auto results = resolver.resolve(kBinanceFuturesTestnetHost, kBinanceFuturesTestnetPort);
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        const std::string target =
            std::string{"/fapi/v1/depth?symbol="} + std::string(symbol) +
            "&limit=" + std::to_string(kDepthLimit);

        http::request<http::string_body> request{http::verb::get, target, 11};
        request.set(http::field::host, kBinanceFuturesTestnetHost);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(stream, request);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> response;
        http::read(stream, buffer, response);

        if (response.result() != http::status::ok) {
            return std::nullopt;
        }

        const std::string body = beast::buffers_to_string(response.body().data());
        const simdjson::padded_string padded(body);
        simdjson::ondemand::parser parser;
        auto doc = parser.iterate(padded);
        if (doc.error()) {
            return std::nullopt;
        }

        auto root = doc.get_object();
        if (root.error()) {
            return std::nullopt;
        }

        OrderBookSnapshot snapshot;

        const auto lastUpdateIdResult = root["lastUpdateId"].get_uint64();
        if (lastUpdateIdResult.error()) {
            return std::nullopt;
        }
        snapshot.lastUpdateId = lastUpdateIdResult.value_unsafe();

        auto bidsArrayResult = root["bids"].get_array();
        if (bidsArrayResult.error()) {
            return std::nullopt;
        }
        auto bidsArray = bidsArrayResult.value_unsafe();
        if (!parseLevelArray(bidsArray, snapshot.bids)) {
            return std::nullopt;
        }

        auto asksArrayResult = root["asks"].get_array();
        if (asksArrayResult.error()) {
            return std::nullopt;
        }
        auto asksArray = asksArrayResult.value_unsafe();
        if (!parseLevelArray(asksArray, snapshot.asks)) {
            return std::nullopt;
        }

        return snapshot;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace nkm
