#pragma once
#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <utility>

#include "Constants.hpp"
#include "Order.hpp"
#include "OrderPool.hpp"

class Orderbook
{
public:
    void processRequest(const OrderRequest& request);
    size_t size() { return size_; };
    explicit Orderbook(size_t maxOrders = 10000000)
        : orderPool_(maxOrders) {

          };

private:
    void addOrder(const Order& order);
    void cancelOrder(const OrderId& orderId);
    void modifyOrder(const Order& order);
    void matchOrders(OrderPointer newOrder);

    OrderPool<Order> orderPool_;

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderPointer> orders_;

    size_t size_{0};
};
