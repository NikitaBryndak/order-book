#pragma once
#include <stdexcept>
#include <map>
#include <unordered_map>
#include <vector>

#include "Orderbook.h"
#include "Order.h"
#include "Constants.h"
#include "LevelInfo.h"
#include "Trade.h"


Trades Orderbook::AddOrder(OrderPointer order)
        {
            if (orders_.count(order->GetOrderId()))
            {
                return { };
            }

            if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            {
                return { };
            }

            if (order->GetOrderType() == OrderType::Market)
            {
                if (order->GetSide() == Side::Buy && !asks_.empty()) {
                    const auto& [worstAskPrice, _] = *asks_.rbegin();
                    order->ToGoodTillCancel(worstAskPrice);

                } else if (order->GetSide() == Side::Sell && !bids_.empty()) {
                    const auto& [worstBidPrice, _] = *bids_.rbegin();
                    order->ToGoodTillCancel(worstBidPrice);
                } else {
                    return { };
                }
            }

            OrderPointers::iterator iterator;

            if (order->GetSide() == Side::Buy)
            {
                auto& orders = bids_[order->GetPrice()];
                orders.push_back(order);
                iterator = std::prev(orders.end());
            }
            else if (order->GetSide() == Side::Sell)
            {
                auto& orders = asks_[order->GetPrice()];
                orders.push_back(order);
                iterator = std::prev(orders.end());
            }

            orders_.insert({order->GetOrderId(),OrderEntry{order,iterator}});
            return MatchOrders();
        }

void Orderbook::CancelOrder(OrderId orderId)
        {
            if (!orders_.count(orderId))
            {
                return;
            }

            const auto& [order, orderIterator] = orders_.at(orderId);
            orders_.erase(orderId);

            if (order->GetSide() == Side::Sell)
            {
                Price price = order->GetPrice();
                auto& orders = asks_.at(price);
                orders.erase(orderIterator);
                if (orders.empty())
                {
                    asks_.erase(price);
                }
            }
            else
            {
                Price price = order->GetPrice();
                auto& orders = bids_.at(price);
                orders.erase(orderIterator);
                if (orders.empty())
                {
                    bids_.erase(price);
                }
            }

        }

Trades Orderbook::ModifyOrder(OrderModify order)
{
    if (!orders_.count(order.GetOrderId()))
    {
        return { };
    }

    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));


}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders, LevelInfos& levelInfos)
    {
        Quantity totalQuantity = 0;
        for (const auto& order : orders)
        {
            totalQuantity += order->GetRemainingQuantity();
        }
        levelInfos.push_back(LevelInfo{price, totalQuantity});
    };


    for (const auto& [price, orders] : bids_)
    {
        CreateLevelInfos(price, orders, bidInfos);
    }
    for (const auto& [price, orders] : asks_)
    {
        CreateLevelInfos(price, orders, askInfos);
    }
    return OrderbookLevelInfos{bidInfos, askInfos};
}

bool Orderbook::CanMatch(Side side, Price price) const
{
    if (side == Side::Buy)
    {
        if (asks_.empty())
        {
            return false;
        }
        const auto& [bestAsk, _] = *asks_.begin();
        return price >= bestAsk;

    }
    else if (side == Side::Sell)
    {
        if (bids_.empty())
        {
            return false;
        }
        const auto& [bestBid, _] = *bids_.begin();
        return price <= bestBid;
    }
    return false;
}

Trades Orderbook::MatchOrders()
{
    Trades trades;
    trades.reserve(orders_.size());

    while (true)
    {
        if (bids_.empty() || asks_.empty())
        {
            break;
        }
        auto& [bidPrice, bids] = *bids_.begin();
        auto& [askPrice, asks] = *asks_.begin();

        if (bidPrice < askPrice)
        {
            break;
        }

        while (bids.size() && asks.size())
        {
            auto& bid = bids.front();
            auto& ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->isFilled()){
                bids.pop_front();
                orders_.erase(bid->GetOrderId());

            }

            if (ask->isFilled()){
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            }

            if (bids.empty())
            {
                bids_.erase(bidPrice);
            }

            if (asks.empty())
            {
                asks_.erase(askPrice);
            }

            trades.push_back(Trade{
                TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity},
                TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity}
            });
        }
    }
    if (!bids_.empty())
    {
        auto& [_, bids] = *bids_.begin();
        auto& order = bids.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
        {
            CancelOrder(order->GetOrderId());
        }
    }

    if (!asks_.empty())
    {
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();
        if (order->GetOrderType() == OrderType::FillAndKill){
            CancelOrder(order->GetOrderId());
        }
    }

    return trades;
}

std::size_t Orderbook::Size() const { return orders_.size(); }
