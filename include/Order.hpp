#pragma once
#include <cstdint>
#include <algorithm>
#include <list>
#include <iostream>
#include <memory>

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
    const Price getPrice() const { return price_; }
    const Quantity getQuantity() const { return remainingQuantity_; }
    const Quantity getInitialQuantity() const { return initialQuantity_; }
    const Side getSide() const { return side_; }
    const OrderId getOrderId() const { return orderId_; }
    const bool isValid() const {return valid_; }

    // Public API
    const bool isFilled() const { return getQuantity() == 0; }
    void Fill(Quantity amount) { remainingQuantity_ -= std::min(remainingQuantity_, amount); }
    void cancel() { valid_ = false; }

    // Overloading
    friend std::ostream &operator<<(std::ostream &out, const Order &order);


private:
    const Price price_;
    const Quantity initialQuantity_;
    Quantity remainingQuantity_;
    const OrderId orderId_;
    const Side side_;
    bool valid_ = true; // used to mark orders as ghosts for better cache locality
};

using OrderPointer = std::shared_ptr<Order>;
using OrderList = std::list<OrderPointer>;