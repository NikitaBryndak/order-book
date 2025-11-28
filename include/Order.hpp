#pragma once
#include <cstdint>
#include <algorithm>
#include <list>
#include <iostream>

#include "Constants.hpp"

class Order
{
public:
    Order(const OrderId orderId, const Price price, const Quantity quantity, const Side side) : orderId_(orderId),
                                                                                                price_(price),
                                                                                                initialQuantity_(quantity),
                                                                                                remainingQuantity_(quantity),
                                                                                                side_(side)
    {
    }

    // Getters and Setters
    const Price getPrice() const { return price_; };
    const Quantity getQuantity() const { return remainingQuantity_; };
    const Quantity getInitialQuantity() const { return initialQuantity_; };
    const Side getSide() const { return side_; };
    const OrderId getOrderId() const { return orderId_; };

    // Public API
    const bool isFilled() const { return getQuantity() == 0; };
    void Fill(Quantity amount) { remainingQuantity_ -= std::min(remainingQuantity_, amount); };

    // Overloading
    friend std::ostream &operator<<(std::ostream &out, const Order &order);


private:
    const Price price_;
    const Quantity initialQuantity_;
    Quantity remainingQuantity_;
    const OrderId orderId_;
    const Side side_;
};

using OrderList = std::list<Order>;