#include <stdexcept>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

#include "order_book/Orderbook.h"
#include "order_book/Constants.h"
#include "order_book/LevelInfo.h"
#include "order_book/Order.h"
#include "order_book/Trade.h"

Orderbook::Orderbook() = default;

Orderbook::~Orderbook() = default;

Trades Orderbook::AddOrder(OrderPointer order)
{
    if (orders_.count(order->GetOrderId()))
    {
        return {};
    }

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
    {
        return {};
    }

    if (order->GetOrderType() == OrderType::Market)
    {
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto &[worstAskPrice, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAskPrice);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto &[worstBidPrice, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBidPrice);
        }
        else
        {
            return {};
        }
    }

    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetRemainingQuantity()))
    {
        return {};
    }

    OrderPointers::iterator iterator;

    if (order->GetSide() == Side::Buy)
    {
        auto &orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
    }
    else if (order->GetSide() == Side::Sell)
    {
        auto &orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
    }

    orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});

    OnOrderAdded(order);
    return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId)
{
    if (!orders_.count(orderId))
    {
        return;
    }

    const auto &[order, orderIterator] = orders_.at(orderId);
    orders_.erase(orderId);

    if (order->GetSide() == Side::Sell)
    {
        Price price = order->GetPrice();
        auto &orders = asks_.at(price);
        orders.erase(orderIterator);
        if (orders.empty())
        {
            asks_.erase(price);
        }
    }
    else
    {
        Price price = order->GetPrice();
        auto &orders = bids_.at(price);
        orders.erase(orderIterator);
        if (orders.empty())
        {
            bids_.erase(price);
        }
    }
    OnOrderCancelled(order);
}

Trades Orderbook::ModifyOrder(OrderModify order)
{
    if (!orders_.count(order.GetOrderId()))
    {
        return {};
    }

    const auto &[existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers &orders, LevelInfos &levelInfos)
    {
        Quantity totalQuantity = 0;
        for (const auto &order : orders)
        {
            totalQuantity += order->GetRemainingQuantity();
        }
        levelInfos.push_back(LevelInfo{price, totalQuantity});
    };

    for (const auto &[price, orders] : bids_)
    {
        CreateLevelInfos(price, orders, bidInfos);
    }
    for (const auto &[price, orders] : asks_)
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
        const auto &[bestAsk, _] = *asks_.begin();
        return price >= bestAsk;
    }
    else if (side == Side::Sell)
    {
        if (bids_.empty())
        {
            return false;
        }
        const auto &[bestBid, _] = *bids_.begin();
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
        auto &[bidPrice, bids] = *bids_.begin();
        auto &[askPrice, asks] = *asks_.begin();

        if (bidPrice < askPrice)
        {
            break;
        }

        while (bids.size() && asks.size())
        {
            auto &bid = bids.front();
            auto &ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->isFilled())
            {
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            }

            if (ask->isFilled())
            {
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
                TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
                TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}});

            std::cout << trades.back() << std::endl;

            OnOrderMatched(bid->GetPrice(), quantity, bid->isFilled());
            OnOrderMatched(ask->GetPrice(), quantity, ask->isFilled());
        }
    }
    if (!bids_.empty())
    {
        auto &[_, bids] = *bids_.begin();
        auto &order = bids.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
        {
            CancelOrder(order->GetOrderId());
        }
    }

    if (!asks_.empty())
    {
        auto &[_, asks] = *asks_.begin();
        auto &order = asks.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
        {
            CancelOrder(order->GetOrderId());
        }
    }

    return trades;
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
{
    UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
    auto &data = data_[price];

    data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1
                                                                                               : 0;

    if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
    {
        data.quantity_ -= quantity;
    }
    else
    {
        data.quantity_ += quantity;
    }

    if (data.count_ == 0)
    {
        data_.erase(price);
    }
}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;

    if (side == Side::Buy)
    {
        for (const auto &[askPrice, orders] : asks_)
        {
            if (askPrice > price)
                break;

            auto it = data_.find(askPrice);
            if (it == data_.end())
                continue;

            const auto &levelData = it->second;
            if (quantity <= levelData.quantity_)
                return true;

            quantity -= levelData.quantity_;
        }
    }
    else // Side::Sell
    {
        for (const auto &[bidPrice, orders] : bids_)
        {
            if (bidPrice < price)
                break;

            auto it = data_.find(bidPrice);
            if (it == data_.end())
                continue;

            const auto &levelData = it->second;
            if (quantity <= levelData.quantity_)
                return true;

            quantity -= levelData.quantity_;
        }
    }

    return false;
}

std::size_t Orderbook::Size() const { return orders_.size(); }
