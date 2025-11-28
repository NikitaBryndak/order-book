#pragma once
#include <map>
#include <list>
#include <functional>

#include "Order.hpp"

class Orderbook
{
public:
    void addOrder(const Order &order);
    void matchOrders();
    size_t size();

private:
    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
};