#pragma once
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <utility>
#include <atomic>

#include "Constants.hpp"
#include "Order.hpp"

struct OrderRequest
{
    RequestType type;
    Order order;
};


class Orderbook
{
public:
    void processRequest(const OrderRequest &request);
    size_t size() { return size_; };

private:
    void addOrder(const Order &order);
    void cancelOrder(const OrderId &orderId);
    void modifyOrder(const Order &order);
    void matchOrders(OrderPointer newOrder);

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderPointer> orders_;

    size_t size_{0};
};
