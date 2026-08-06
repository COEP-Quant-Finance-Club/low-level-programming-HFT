#include "exchange/BinanceFuturesClient.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
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

constexpr char kBinanceTestnetHost[] = "fstream.binance.com";
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

std::string buildSubscribePayload(std::string_view symbol)
{
    const std::string normalized = normalizeSymbol(symbol);
    return std::string{"{\"method\":\"SUBSCRIBE\",\"params\":[\""} + normalized + "@trade\"],\"id\":1}";
}

class WebSocketSession
{
public:
    WebSocketSession()
        : resolver_(io_)
        , stream_(io_, ctx_)
    {
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
            const auto results = resolver_.resolve(kBinanceTestnetHost, kBinanceTestnetPort);
            net::connect(stream_.next_layer().lowest_layer(), results.begin(), results.end());

            if (!SSL_set_tlsext_host_name(stream_.next_layer().native_handle(), kBinanceTestnetHost))
            {
                throw std::runtime_error("SSL_set_tlsext_host_name failed");
            }

            stream_.next_layer().handshake(ssl::stream_base::client);
            stream_.handshake(kBinanceTestnetHost, kBinanceWsPath);
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
            const std::string payload = buildSubscribePayload(symbol);
            stream_.write(net::buffer(payload));
            return true;
        }
        catch (std::exception const& ex) {
            std::cerr << "BinanceFuturesClient: subscribe failed: " << ex.what() << '\n';
            return false;
        }
    }

    void run()
    {
        if (!connected_)
        {
            return;
        }

        try {
            beast::flat_buffer buffer;
            while (true)
            {
                stream_.read(buffer);
                std::cout << beast::buffers_to_string(buffer.data()) << std::endl;
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

bool BinanceFuturesClient::subscribeTrades(std::string_view symbol)
{
    if (!impl_->session)
    {
        return false;
    }

    return impl_->session->subscribe(symbol);
}

void BinanceFuturesClient::run()
{
    if (!impl_->session)
    {
        return;
    }

    impl_->session->run();
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
