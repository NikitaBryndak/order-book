#include "Orderbook.hpp"
#include "Order.hpp"
#include "Constants.hpp"

void Orderbook::matchOrders()
{
    while (true)
    {
        if (asks_.empty() || bids_.empty())
        {
            break;
        }

        cleanLevels();

        auto &[bestBidPrice, bids] = *bids_.begin();
        auto &[bestAskPrice, asks] = *asks_.begin();

        if (bestBidPrice < bestAskPrice)
            break;


        Quantity fillQuantity = std::min(bids.front()->getQuantity(), asks.front()->getQuantity());

        bids.front()->Fill(fillQuantity);
        asks.front()->Fill(fillQuantity);

        // std::cout << *bids.begin();
        // std::cout << *asks.begin();

        if (bids.front()->isFilled())
        {
            orders_.erase(bids.front()->getOrderId());
            bids.pop_front();
            size_--;
        }

        if (asks.front()->isFilled())
        {
            orders_.erase(asks.front()->getOrderId());
            asks.pop_front();
            size_--;
        }

        if (bids.empty())
        {
            bids_.erase(bids_.begin());
        }

        if (asks.empty())
        {
            asks_.erase(asks_.begin());
        }
    }
};

void Orderbook::addOrder(const Order &order)
{
    OrderPointer orderPtr = std::make_shared<Order>(order);
    std::lock_guard<std::mutex> guard(mutex_);

    if (order.getSide() == Side::Buy)
    {
        bids_[order.getPrice()].push_back(orderPtr);
    }
    else
    {
        asks_[order.getPrice()].push_back(orderPtr);
    }
    orders_[order.getOrderId()] = orderPtr;
    size_++;
    matchOrders();
}

void Orderbook::cancelOrder(const OrderId &orderId)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (orders_.count(orderId))
    {
        orders_[orderId]->cancel();
        orders_.erase(orderId);
        size_--;
    }
}

void Orderbook::cleanLevels()
{
    // Clean bids
    {
        while (!bids_.empty())
        {
            if (bids_.begin()->second.empty())
            {
                bids_.erase(bids_.begin());
                continue;
            }

            if (!bids_.begin()->second.front()->isValid())
            {
                bids_.begin()->second.pop_front();
                continue;
            }

            break;
        }
    }

    // Clean asks
    {
        while (!asks_.empty())
        {
            if (asks_.begin()->second.empty())
            {
                asks_.erase(asks_.begin());
                continue;
            }

            if (!asks_.begin()->second.front()->isValid())
            {
                asks_.begin()->second.pop_front();
                continue;
            }

            break;
        }
    }
}

size_t Orderbook::size() {
    std::lock_guard<std::mutex> guard(mutex_);
    return size_;
}