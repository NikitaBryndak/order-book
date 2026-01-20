#pragma once
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <utility>
#include <atomic>

#include "Constants.hpp"
#include "Order.hpp"

class Orderbook
{
public:
    void addOrder(const Order &order);
    void cancelOrder(const OrderId &orderId);
    void modifyOrder(const OrderId &orderId, const Order &order);

    size_t size() { return size_; };

private:
    void matchOrders(OrderPointer newOrder);
    // void cleanLevels();

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderPointer> orders_;

    mutable std::mutex mutex_;
    std::atomic<size_t> size_{0};
};
