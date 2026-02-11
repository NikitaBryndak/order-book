#include "Orderbook/Orderbook.hpp"
#include "Orderbook/Order.hpp"
#include "Constants.hpp"
#include "utils.hpp"
#include "Config.hpp"

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <set>
#include <cstdlib>
#include <sched.h>
#include <pthread.h>

void Orderbook::matchOrders(OrderPointer newOrder)
{
    if (newOrder->getSide() == Side::Buy)
    {
        while (true)
        {
            if (asks_.empty() || newOrder->isFilled())
            {
                break;
            }

            auto &[bestAskPrice, asks] = *asks_.begin();

            // Lazy cleanup of cancelled orders
            if (!asks.front()->isValid()) [[unlikely]]
            {
                auto cancelled = asks.front();
                asks.pop_front();

                if (asks.empty())
                {
                    askLevels_.erase(bestAskPrice);
                    asks_.erase(asks_.begin());
                }
                orderPool_.release(cancelled);
                continue;
            }

            if (newOrder->getPrice() < bestAskPrice)
            {
                break;
            }

            Quantity fillQuantity = std::min(newOrder->getQuantity(), asks.front()->getQuantity());

            newOrder->Fill(fillQuantity);
            asks.front()->Fill(fillQuantity);
            askLevels_[bestAskPrice] -= fillQuantity;

            if (asks.front()->isFilled())
            {
                OrderPointer filled = asks.front();
                orders_.erase(filled->getOrderId());
                asks.pop_front();
                size_--;
                orderPool_.release(filled);
            }

            if (asks.empty())
            {
                askLevels_.erase(bestAskPrice);
                asks_.erase(asks_.begin());
            }
        }
    }
    else
    {
        while (true)
        {
            if (bids_.empty() || newOrder->isFilled())
            {
                break;
            }

            auto &[bestBidPrice, bids] = *bids_.begin();

            // Lazy cleanup of cancelled orders
            if (!bids.front()->isValid())
            {
                auto cancelled = bids.front();
                bids.pop_front();
                if (bids.empty())
                {
                    bidLevels_.erase(bestBidPrice);
                    bids_.erase(bids_.begin());
                }
                orderPool_.release(cancelled);
                continue;
            }

            if (newOrder->getPrice() > bestBidPrice)
            {
                break;
            }

            Quantity fillQuantity = std::min(newOrder->getQuantity(), bids.front()->getQuantity());

            newOrder->Fill(fillQuantity);
            bids.front()->Fill(fillQuantity);
            bidLevels_[bestBidPrice] -= fillQuantity;

            if (bids.front()->isFilled())
            {
                OrderPointer filled = bids.front();
                orders_.erase(filled->getOrderId());
                bids.pop_front();
                size_--;
                orderPool_.release(filled);
            }

            if (bids.empty())
            {
                bidLevels_.erase(bestBidPrice);
                bids_.erase(bids_.begin());
            }
        }
    }
};

void Orderbook::addOrder(const Order &order)
{
    OrderPointer orderPtr = orderPool_.acquire(
        order.getOrderId(),
        order.getOrderType(),
        order.getPrice(),
        order.getQuantity(),
        order.getSide()
    );

    if (!orderPtr)
    {
        throw std::runtime_error("Out of orders");
    }

    matchOrders(orderPtr);

    if (!orderPtr->isFilled() && order.getOrderType() == OrderType::GoodTillCancel)
    {
        if (order.getSide() == Side::Buy)
        {
            bids_[order.getPrice()].push_back(orderPtr);
            bidLevels_[order.getPrice()] += order.getQuantity();
        }
        else
        {
            asks_[order.getPrice()].push_back(orderPtr);
            askLevels_[order.getPrice()] += order.getQuantity();
        }

        orders_[order.getOrderId()] = orderPtr;
        size_++;
    }
    else
    {
        orderPool_.release(orderPtr);
    }
}

void Orderbook::cancelOrder(const OrderId &orderId)
{
    if (orders_.count(orderId))
    {
        OrderPointer order = orders_[orderId];
        Price price = order->getPrice();
        Quantity qty = order->getQuantity();

        if (order->getSide() == Side::Buy) {
            bidLevels_[price] -= qty;
            if (bidLevels_[price] == 0) bidLevels_.erase(price);
        } else {
            askLevels_[price] -= qty;
            if (askLevels_[price] == 0) askLevels_.erase(price);
        }

        order->cancel();
        orders_.erase(orderId);
        size_--;
    }
}

void Orderbook::modifyOrder(const Order &order)
{
    this->cancelOrder(order.getOrderId());
    this->addOrder(order);
}

void Orderbook::submitRequest(OrderRequest &request)
{
    buffer_.push(std::move(request));
}

void Orderbook::processLoop()
{
    while (true)
    {
        OrderRequest request = buffer_.pop();
        switch (request.type)
        {
        case (RequestType::Add):
            this->addOrder(request.order);
            if (verbose_) printState();
            break;

        case (RequestType::Cancel):
            this->cancelOrder(request.order.getOrderId());
            if (verbose_) printState();
            break;

        case (RequestType::Modify):
            this->modifyOrder(request.order);
            if (verbose_) printState();
            break;
        case (RequestType::Stop):
            return;
        }
    }
}

void Orderbook::printState() const
{
    system("clear");

    int totalWidth = 8 + Config::barWidth + 3 + 9 + 3 + Config::barWidth + 8;
    std::string title = "Real-Time Orderbook State";
    int titlePadding = (totalWidth > (int)title.length()) ? (totalWidth - title.length()) / 2 : 0;
    std::cout << std::string(titlePadding, ' ') << title << "\n";
    std::cout << std::string(titlePadding, ' ') << std::string(title.length(), '=') << "\n\n";

    // Collect all unique prices
    std::set<Price> allPrices;
    Quantity maxQty = 0;
    for (const auto& [price, q] : bidLevels_)
    {
        allPrices.insert(price);
        if (q > maxQty) maxQty = q;
    }
    for (const auto& [price, q] : askLevels_)
    {
        allPrices.insert(price);
        if (q > maxQty) maxQty = q;
    }

    // Header
    auto center = [](std::string s, int w) {
        int p = (w - s.length()) / 2;
        return std::string(p, ' ') + s + std::string(w - p - s.length(), ' ');
    };

    std::cout << center("BidQty", 8) << center("Volume", Config::barWidth)
              << " | " << center("Price", 9) << " | "
              << center("Volume", Config::barWidth) << center("AskQty", 8) << "\n";

    std::cout << std::string(totalWidth, '-') << "\n";

    for (auto it = allPrices.rbegin(); it != allPrices.rend(); ++it) {
        Price price = *it;
        Quantity bidQty = bidLevels_.count(price) ? bidLevels_.at(price) : 0;
        Quantity askQty = askLevels_.count(price) ? askLevels_.at(price) : 0;

        // Skip rows with no volume
        if (bidQty == 0 && askQty == 0) continue;

        // Create volume bars
        int bidBarLength = maxQty > 0 ? (bidQty * Config::barWidth) / maxQty : 0;
        int askBarLength = maxQty > 0 ? (askQty * Config::barWidth) / maxQty : 0;

        // Bid: qty right-aligned in 6, then spaces + green bars (right-aligned)
        std::string bidQtyStr = bidQty > 0 ? std::to_string(bidQty) : "";
        std::string bidSpaces = std::string(Config::barWidth - bidBarLength, ' ');
        std::string bidBars = std::string(bidBarLength, '#');

        // Ask: red bars + spaces, then qty left-aligned in 6
        std::string askBars = std::string(askBarLength, '#');
        std::string askSpaces = std::string(Config::barWidth - askBarLength, ' ');
        std::string askQtyStr = askQty > 0 ? std::to_string(askQty) : "";

        // Price centered in 8
        std::string priceStr = std::to_string(price);
        int pricePadding = (9 - priceStr.length()) / 2;
        std::string pricePadded = std::string(pricePadding, ' ') + priceStr + std::string(9 - pricePadding - priceStr.length(), ' ');

        // Output the row
        std::cout << std::left << std::setw(8) << bidQtyStr
                  << bidSpaces << bidBars << " | "
                  << pricePadded << " | "
                  << askBars << askSpaces
                  << std::left << std::setw(8) << askQtyStr << "\n";
    }

    std::cout << "\nTotal Resting Orders: " << size_ << "\n";
    std::cout << std::flush;
}

Price Orderbook::topBidPrice() const
{
    const auto [p, _]  = *(bids_.begin());
    return p;
}

Price Orderbook::topBidPrice() const
{
    const auto [p, _]  = *(asks_.begin());
    return p;
}

Orderbook::Orderbook(size_t maxOrders, int coreId, bool verbose) : orderPool_(maxOrders), buffer_(nextPowerOf2(maxOrders)), verbose_(verbose)
{
    workerThread_ = std::thread(&Orderbook::processLoop, this);

    if (coreId >= 0)
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreId, &cpuset);

        int rc = pthread_setaffinity_np(workerThread_.native_handle(), sizeof(cpu_set_t), &cpuset);

        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        }
    }
}

Orderbook::~Orderbook()
{
    OrderRequest stop;
    stop.type = RequestType::Stop;

    submitRequest(stop);

    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}
