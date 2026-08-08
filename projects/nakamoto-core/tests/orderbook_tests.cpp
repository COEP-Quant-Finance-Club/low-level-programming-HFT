#include "orderbook/OrderBook.hpp"

#include "market_data/DepthEvent.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    { // inserting a bid
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 1;
        event.symbol = "BTCUSDT";
        event.bids.push_back(nkm::PriceLevel{100.0, 10.0});
        book.applyDepthUpdate(event);

        assert(book.bestBid() == 100.0);
        assert(book.bidQuantity(100.0) == 10.0);
    }

    { // inserting an ask
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 2;
        event.symbol = "BTCUSDT";
        event.asks.push_back(nkm::PriceLevel{101.0, 5.0});
        book.applyDepthUpdate(event);

        assert(book.bestAsk() == 101.0);
        assert(book.askQuantity(101.0) == 5.0);
    }

    { // updating an existing level
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 3;
        event.symbol = "BTCUSDT";
        event.bids.push_back(nkm::PriceLevel{100.0, 20.0});
        book.applyDepthUpdate(event);

        assert(book.bestBid() == 100.0);
        assert(book.bidQuantity(100.0) == 20.0);

        nkm::DepthEvent update;
        update.symbol = "BTCUSDT";
        update.bids.push_back(nkm::PriceLevel{100.0, 25.0});
        book.applyDepthUpdate(update);

        assert(book.bidQuantity(100.0) == 25.0);
    }

    { // updating an existing ask
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 31;
        event.symbol = "BTCUSDT";
        event.asks.push_back(nkm::PriceLevel{101.0, 5.0});
        book.applyDepthUpdate(event);

        assert(book.bestAsk() == 101.0);
        assert(book.askQuantity(101.0) == 5.0);

        nkm::DepthEvent update;
        update.symbol = "BTCUSDT";
        update.asks.push_back(nkm::PriceLevel{101.0, 9.0});
        book.applyDepthUpdate(update);

        assert(book.askQuantity(101.0) == 9.0);
    }

    { // deleting a level with quantity zero
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 4;
        event.symbol = "BTCUSDT";
        event.bids.push_back(nkm::PriceLevel{100.0, 10.0});
        book.applyDepthUpdate(event);

        nkm::DepthEvent deleteEvent;
        deleteEvent.symbol = "BTCUSDT";
        deleteEvent.bids.push_back(nkm::PriceLevel{100.0, 0.0});
        book.applyDepthUpdate(deleteEvent);

        assert(book.bidQuantity(100.0) == 0.0);
        assert(book.bestBid() == 0.0);
    }

    { // deleting an ask level with quantity zero
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 41;
        event.symbol = "BTCUSDT";
        event.asks.push_back(nkm::PriceLevel{101.0, 10.0});
        book.applyDepthUpdate(event);

        nkm::DepthEvent deleteEvent;
        deleteEvent.symbol = "BTCUSDT";
        deleteEvent.asks.push_back(nkm::PriceLevel{101.0, 0.0});
        book.applyDepthUpdate(deleteEvent);

        assert(book.askQuantity(101.0) == 0.0);
        assert(book.bestAsk() == 0.0);
    }

    { // best bid calculation
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 5;
        event.symbol = "BTCUSDT";
        event.bids.push_back(nkm::PriceLevel{95.0, 2.0});
        event.bids.push_back(nkm::PriceLevel{110.0, 6.0});
        book.applyDepthUpdate(event);

        assert(book.bestBid() == 110.0);
    }

    { // best ask calculation
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.eventTimestamp = 6;
        event.symbol = "BTCUSDT";
        event.asks.push_back(nkm::PriceLevel{120.0, 2.0});
        event.asks.push_back(nkm::PriceLevel{105.0, 1.0});
        book.applyDepthUpdate(event);

        assert(book.bestAsk() == 105.0);
    }

    { // spread calculation
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.bids.push_back(nkm::PriceLevel{100.0, 1.0});
        event.asks.push_back(nkm::PriceLevel{105.0, 1.0});
        book.applyDepthUpdate(event);

        assert(book.spread() == 5.0);
    }

    { // mid price calculation
        nkm::OrderBook book;
        nkm::DepthEvent event;
        event.bids.push_back(nkm::PriceLevel{100.0, 1.0});
        event.asks.push_back(nkm::PriceLevel{105.0, 1.0});
        book.applyDepthUpdate(event);

        assert(book.midPrice() == 102.5);
    }

    return 0;
}
