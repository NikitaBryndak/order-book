#pragma once

#include <stdexcept>
#include <memory>
#include <list>

#include "Constants.h"


// TODO: Add GoodForDay
enum struct OrderType { GoodTillCancel, FillAndKill, Market, FillOrKill};

class Order {
    public:
        Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) :
            orderType_{orderType},
            orderId_{orderId},
            side_{side},
            price_{price},
            initialQuantity_{quantity},
            remainingQuantity_{quantity}
        { }

        Order(OrderId orderId, Side side, Quantity quantity) :
            Order(OrderType::Market, orderId, side, InvalidPrice, quantity)
        { }

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

        void ToGoodTillCancel(Price price) {
            if (GetOrderType() != OrderType::Market) {
                throw std::logic_error("Only market orders can be converted to good till cancel orders");
            }
            price_ = price;
            orderType_ = OrderType::GoodTillCancel;
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

        OrderPointer ToOrderPointer(OrderType type) {
            return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
        }

    private:
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity quantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;