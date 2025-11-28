#include "Orderbook.hpp"
#include "Order.hpp"
#include "Constants.hpp"

void Orderbook::matchOrders()
{
    // Check if empty -> Can not compare
    if (bids_.empty() || asks_.empty())
        return;

    // Get best current prices
    auto &[bestBidPrice, _] = *bids_.begin();
    auto &[bestAskPrice, _] = *asks_.begin();

    // Check weather overlap exists
    if (bestBidPrice < bestAskPrice)
        return;
    else
    {
        while (true)
        {
            if (asks_.empty() || bids_.empty())
            {
                break;
            }

            auto &[bestBidPrice, bids] = *bids_.begin();
            auto &[bestAskPrice, asks] = *asks_.begin();

            if (bestBidPrice < bestAskPrice)
                break;

            Quantity fillQuantity = std::min(bids.begin()->getQuantity(), asks.begin()->getQuantity());

            bids.begin()->Fill(fillQuantity);
            asks.begin()->Fill(fillQuantity);

            std::cout << *bids.begin();
            std::cout << *asks.begin();

            if (bids.begin()->isFilled())
            {
                bids.pop_front();
            }

            if (asks.begin()->isFilled())
            {
                asks.pop_front();
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
    }
};

void Orderbook::addOrder(const Order &order)
{
    if (order.getSide() == Side::Buy)
    {
        bids_[order.getPrice()].push_back(order);
    }
    else
    {
        asks_[order.getPrice()].push_back(order);
    }
    matchOrders();
}

size_t Orderbook::size() { return bids_.size() + asks_.size(); }