#pragma once
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <utility>
#include <mutex>

#include "Constants.hpp"
#include "Order.hpp"

class Orderbook
{
public:
    void addOrder(const Order &order);
    void cancelOrder(const OrderId &orderId);

    size_t size();

private:
    void matchOrders();
    void cleanLevels();

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderPointer> orders_;

    mutable std::mutex mutex_;
    size_t size_{0};

};
