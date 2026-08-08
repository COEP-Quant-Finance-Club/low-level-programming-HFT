#include "exchange/BinanceFuturesClient.hpp"
#include "market_data/MarketDataParser.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace nkm {

namespace {

constexpr char kBinanceTestnetHost[] = "stream.binancefuture.com";
constexpr char kBinanceTestnetPort[] = "443";
constexpr char kBinanceWsPath[] = "/ws";

std::string normalizeSymbol(std::string_view symbol)
{
    std::string normalized(symbol);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

std::string buildDepthSubscribePayload(std::string_view symbol)
{
    const std::string normalized = normalizeSymbol(symbol);
    return std::string{"{\"method\":\"SUBSCRIBE\",\"params\":[\""} + normalized + "@depth\"],\"id\":1}";
}

class WebSocketSession
{
public:
    WebSocketSession()
        : resolver_(io_)
        , stream_(io_, ctx_)
    {
        std::cout << "DEBUG: Creating io_context\n";
        ctx_.set_default_verify_paths();
        ctx_.set_verify_mode(ssl::verify_peer);
    }

    ~WebSocketSession() = default;

    WebSocketSession(const WebSocketSession&) = delete;
    WebSocketSession& operator=(const WebSocketSession&) = delete;

    WebSocketSession(WebSocketSession&&) = delete;
    WebSocketSession& operator=(WebSocketSession&&) = delete;

    bool connect()
    {
        try {
            std::cout << "DEBUG: Resolving hostname\n";
            const auto results = resolver_.resolve(kBinanceTestnetHost, kBinanceTestnetPort);
            std::cout << "DEBUG: Resolved hostname\n";

            std::cout << "DEBUG: TCP connect\n";
            net::connect(stream_.next_layer().lowest_layer(), results.begin(), results.end());
            std::cout << "DEBUG: TCP connect complete\n";

            if (!SSL_set_tlsext_host_name(stream_.next_layer().native_handle(), kBinanceTestnetHost))
            {
                throw std::runtime_error("SSL_set_tlsext_host_name failed");
            }

            std::cout << "DEBUG: SSL handshake\n";
            stream_.next_layer().handshake(ssl::stream_base::client);
            std::cout << "DEBUG: SSL handshake complete\n";

            std::cout << "DEBUG: WebSocket handshake\n";
            stream_.handshake(kBinanceTestnetHost, kBinanceWsPath);
            std::cout << "DEBUG: WebSocket handshake complete\n";

            connected_ = true;
            return true;
        }
        catch (std::exception const& ex) {
            std::cerr << "BinanceFuturesClient: connect failed: " << ex.what() << '\n';
            connected_ = false;
            return false;
        }
    }

    bool subscribe(std::string_view symbol)
    {
        if (!connected_)
        {
            return false;
        }

        try {
            const std::string payload = buildDepthSubscribePayload(symbol);
            stream_.write(net::buffer(payload));
            std::cout << "DEBUG: Sending subscription\n";
            std::cout << "DEBUG: Subscription sent\n";
            return true;
        }
        catch (std::exception const& ex) {
            std::cerr << "BinanceFuturesClient: subscribe failed: " << ex.what() << '\n';
            return false;
        }
    }

    void run(const BinanceFuturesClient::DepthEventSink& sink)
    {
        if (!connected_)
        {
            return;
        }

        try {
            beast::flat_buffer buffer;
            MarketDataParser parser;

            while (true)
            {
                stream_.read(buffer);

                const std::string payload = beast::buffers_to_string(buffer.data());
                const std::string_view payloadView(payload);

                const bool looksLikeAck = payloadView.find("\"result\":null") != std::string_view::npos || payloadView.find("\"id\":1") != std::string_view::npos;
                if (looksLikeAck) {
                    std::cout << "DEBUG: Subscription acknowledged\n";
                }
                else if (const auto parsed = parser.parseDepthEvent(payloadView); parsed.has_value()) {
                    sink(*parsed);
                }
                else {
                    std::cerr << "BinanceFuturesClient: malformed or non-depth WebSocket message ignored\n";
                }

                buffer.consume(buffer.size());
            }
        }
        catch (beast::system_error const& se) {
            std::cerr << "BinanceFuturesClient: websocket read failed: " << se.what() << '\n';
        }
        catch (std::exception const& ex) {
            std::cerr << "BinanceFuturesClient: run failed: " << ex.what() << '\n';
        }
    }

    void disconnect()
    {
        try {
            if (connected_ && stream_.next_layer().lowest_layer().is_open()) {
                beast::error_code ec;
                stream_.close(websocket::close_code::normal, ec);
            }
        }
        catch (std::exception const&) {
            // Best-effort shutdown; the socket will be cleaned up by the destructor.
        }

        connected_ = false;
    }

private:
    net::io_context io_;
    ssl::context ctx_{ssl::context::tlsv12_client};
    tcp::resolver resolver_;
    websocket::stream<ssl::stream<tcp::socket>> stream_;
    bool connected_{false};
};

} // namespace

struct BinanceFuturesClient::Impl
{
    std::unique_ptr<WebSocketSession> session;
};

BinanceFuturesClient::BinanceFuturesClient()
    : impl_(std::make_unique<Impl>())
{
}

BinanceFuturesClient::~BinanceFuturesClient() = default;

BinanceFuturesClient::BinanceFuturesClient(BinanceFuturesClient&&) noexcept = default;
BinanceFuturesClient& BinanceFuturesClient::operator=(BinanceFuturesClient&&) noexcept = default;

bool BinanceFuturesClient::connect()
{
    if (!impl_->session)
    {
        impl_->session = std::make_unique<WebSocketSession>();
    }

    return impl_->session->connect();
}

bool BinanceFuturesClient::subscribeDepth(std::string_view symbol)
{
    if (!impl_->session)
    {
        return false;
    }

    return impl_->session->subscribe(symbol);
}

bool BinanceFuturesClient::subscribeTrades(std::string_view symbol)
{
    return subscribeDepth(symbol);
}

void BinanceFuturesClient::run(const DepthEventSink& sink)
{
    if (!impl_->session)
    {
        return;
    }

    impl_->session->run(sink);
}

void BinanceFuturesClient::disconnect()
{
    if (impl_->session)
    {
        impl_->session->disconnect();
        impl_->session.reset();
    }
}

} // namespace nkm
