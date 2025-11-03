
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <memory>
#include <map>
#include <unordered_map>
#include <deque>
#include <list>
#include <iostream>

enum struct OrderType { GoodTillCancel, FillAndKill };
enum struct Side { Buy, Sell };

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo {
    Price _price;
    Quantity _quantity;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos {
    public:
        OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) : bids_(bids), asks_(asks) {};
        const LevelInfos& GetBids() const { return bids_; };
        const LevelInfos& GetAsks() const { return asks_; };

    private:
        LevelInfos bids_;
        LevelInfos asks_;
};

class Order {
    public:
        Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) :
            orderType_{orderType},
            orderId_{orderId},
            side_{side},
            price_{price},
            initialQuantity_{quantity},
            remainingQuantity_{quantity}
        {

        }

        OrderId GetOrderId() const { return orderId_; }
        Side GetSide() const { return side_; }
        Price GetPrice() const { return price_; }
        OrderType GetOrderType() const { return orderType_; }
        Quantity GetInitialQuantity() const { return initialQuantity_; }
        Quantity GetRemainingQuantity() const { return remainingQuantity_; }
        Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }

        void Fill(Quantity quantity) {
            if (quantity > GetRemainingQuantity()) {
                throw std::logic_error(
                    "Order (" + std::to_string(GetOrderId()) + ") cannot be filled: insufficient quantity"
                );
            }
            remainingQuantity_ -= quantity;
        }

        bool isFilled() const { return GetRemainingQuantity(); }

    private:
        OrderType orderType_;
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity initialQuantity_;
        Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify {
    public:
        OrderModify(OrderId orderId, Side side, Price price, Quantity quantity) :
            orderId_{orderId},
            side_{side},
            price_{price},
            quantity_{quantity} 
        {

        }

        OrderId GetOrderId() const { return orderId_; }
        Side GetSide() const { return side_; }
        Price GetPrice() const { return price_; }
        Quantity GetQuantity() const { return quantity_; } 

        OrderPointer ToOrderPoiner(OrderType type) {
            return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
        }
        
    private:
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity quantity_;
};

struct TradeInfo {
    OrderId orderId_;
    Price _price;
    Quantity quantity_;
};

class Trade {
    public:
        Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade):
            askTrade_ {askTrade},
            bidTrade_ {bidTrade}
        {

        };

        const TradeInfo& GetBidTrade() const { return bidTrade_; }
        const TradeInfo& GetAskTrade() const { return askTrade_; }
        
    private:
        TradeInfo askTrade_;
        TradeInfo bidTrade_;
};

using Trades = std::vector<Trade>;

class Orderbook 
{
    public:
        Trades AddOrder(OrderPointer order)
        {
            if (orders_.contains(order->GetOrderId()))
            {
                return { };
            }

            if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            {
                return { };
            }

            OrderPointers::iterator iterator;

            if (order->GetSide() == Side::Buy)
            {
                auto& orders = bids_[order->GetPrice()];
                orders.push_back(order);
                iterator = std::next(orders.begin(), orders.size() - 1);
            }
            else if (order->GetSide() == Side::Sell)
            {
                auto& orders = asks_[order->GetPrice()];
                orders.push_back(order);
                iterator = std::next(orders.begin(), orders.size() - 1);
            }

            orders_.insert({order->GetOrderId(),OrderEntry{order,iterator}});
            return MatchOrders();
        }

        void CancelOrder(OrderId orderId) 
        {
            if (!orders_.contains(orderId))
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
        
        Trades MatchOrder(OrderModify order)
        {
            if (!orders_.contains(order.GetOrderId()))
            {
                return { };
            }

            const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
            CancelOrder(order.GetOrderId());
            return AddOrder(order.ToOrderPoiner(existingOrder->GetOrderType()));


        }

        std::size_t Size() const { return orders_.size(); }

        OrderbookLevelInfos GetOrderInfos() const
        
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



    private:
        struct OrderEntry 
        {
            OrderPointer order_{nullptr};
            OrderPointers::iterator location_;
        };

        std::map<Price,OrderPointers,std::greater<Price>> bids_;
        std::map<Price,OrderPointers,std::less<Price>> asks_;
        std::unordered_map<OrderId, OrderEntry> orders_;

        bool CanMatch(Side side, Price price) const 
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

        Trades MatchOrders()
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
                    bid->Fill(quantity);
                    
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

};

int main() 
{
    Orderbook orderbook;
    const OrderId orderId = 1;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 19));
    std::cout << orderbook.Size() << '\n';
    orderbook.CancelOrder(orderId);
    std::cout << orderbook.Size() << '\n';
    return 0;
}