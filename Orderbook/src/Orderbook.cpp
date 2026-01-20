#include "Orderbook.hpp"
#include "Order.hpp"
#include "Constants.hpp"

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
            if (!asks.front()->isValid())
            {
                asks.pop_front();
                if (asks.empty())
                {
                    asks_.erase(asks_.begin());
                }
                continue;
            }

            if (newOrder->getPrice() < bestAskPrice)
            {
                break;
            }

            Quantity fillQuantity = std::min(newOrder->getQuantity(), asks.front()->getQuantity());

            newOrder->Fill(fillQuantity);
            asks.front()->Fill(fillQuantity);

            if (asks.front()->isFilled())
            {
                orders_.erase(asks.front()->getOrderId());
                asks.pop_front();
                size_--;
            }

            if (asks.empty())
            {
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
                bids.pop_front();
                if (bids.empty())
                {
                    bids_.erase(bids_.begin());
                }
                continue;
            }

            if (newOrder->getPrice() > bestBidPrice)
            {
                break;
            }

            Quantity fillQuantity = std::min(newOrder->getQuantity(), bids.front()->getQuantity());

            newOrder->Fill(fillQuantity);
            bids.front()->Fill(fillQuantity);

            if (bids.front()->isFilled())
            {
                orders_.erase(bids.front()->getOrderId());
                bids.pop_front();
                size_--;
            }

            if (bids.empty())
            {
                bids_.erase(bids_.begin());
            }
        }
    }
};

void Orderbook::addOrder(const Order &order)
{
    std::lock_guard<std::mutex> guard(mutex_);

    OrderPointer orderPtr = std::make_shared<Order>(order);

    matchOrders(orderPtr);

    if (!orderPtr->isFilled() && order.getOrderType() == OrderType::GoodTillCancel)
    {
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
    }
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

void Orderbook::modifyOrder(const OrderId &orderId, const Order &order)
{
    std::lock_guard<std::mutex> guard(mutex_);

    this->cancelOrder(orderId);
    this->addOrder(order);
}

/* -------------------------------------------------------------------------- */
/*                                   LEGACY                                   */
/* -------------------------------------------------------------------------- */

// void Orderbook::cleanLevels()
// {
//     // Clean bids
//     {
//         while (!bids_.empty())
//         {
//             if (bids_.begin()->second.empty())
//             {
//                 bids_.erase(bids_.begin());
//                 continue;
//             }

//             if (!bids_.begin()->second.front()->isValid())
//             {
//                 bids_.begin()->second.pop_front();
//                 continue;
//             }

//             break;
//         }
//     }

//     // Clean asks
//     {
//         while (!asks_.empty())
//         {
//             if (asks_.begin()->second.empty())
//             {
//                 asks_.erase(asks_.begin());
//                 continue;
//             }

//             if (!asks_.begin()->second.front()->isValid())
//             {
//                 asks_.begin()->second.pop_front();
//                 continue;
//             }

//             break;
//         }
//     }
// }