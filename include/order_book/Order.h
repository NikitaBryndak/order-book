#pragma once

#include <stdexcept>
#include <memory>
#include <list>
#include <iostream>

#include "Constants.h"

class Order;
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;
// TODO: Add GoodForDay
enum struct OrderType
{
    GoodTillCancel,
    FillAndKill,
    Market,
    FillOrKill
};

class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) : orderType_{orderType},
                                                                                             orderId_{orderId},
                                                                                             side_{side},
                                                                                             price_{price},
                                                                                             initialQuantity_{quantity},
                                                                                             remainingQuantity_{quantity}
    {
    }

    Order(OrderId orderId, Side side, Quantity quantity) : Order(OrderType::Market, orderId, side, InvalidPrice, quantity)
    {
    }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }

    void Fill(Quantity quantity);
    void ToGoodTillCancel(Price price);
    bool isFilled() const;

    friend std::ostream &operator<<(std::ostream &os, const Order &order);

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity) : orderId_{orderId},
                                                                              side_{side},
                                                                              price_{price},
                                                                              quantity_{quantity}
    {
    }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type);

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};
