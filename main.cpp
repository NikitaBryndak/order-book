
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <format>
#include <memory>
#include <map>
#include <unordered_map>

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
                throw std::logic_error(std::format("Order ({}) cannot be filled: insufficient quantity", GetOrderId));
            }
            remainingQuantity_ -= quantity;
        }

    private:
        OrderType orderType_;
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity initialQuantity_;
        Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::vector<OrderPointer>;

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

class Orderbook {
    public:

    private:
        struct OrderEntry {
            OrderPointer order_{nullptr};
            OrderPointers::iterator location_;
        };

        std::map<Price,OrderPointers,std::greater<Price>> bids_;
        std::map<Price,OrderPointers,std::less<Price>> asks_;
        std::unordered_map<OrderId, OrderEntry> orders_;
        
};