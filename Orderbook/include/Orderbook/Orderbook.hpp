#pragma once
#include <list>
#include <map>
#include <unordered_map>
#include <thread>

#include "../Constants.hpp"
#include "Order.hpp"
#include "OrderPool.hpp"
#include "RingBuffer.hpp"

class Orderbook
{
public:
    void submitRequest(OrderRequest& request);
    size_t size() const { return size_; };

    explicit Orderbook(size_t maxOrders, int coreId = -1, bool verbose = false);

    Price topBidPrice() const;
    Price topAskPrice() const;

    ~Orderbook();

private:
    void addOrder(const Order& order);
    void cancelOrder(const OrderId& orderId);
    void modifyOrder(const Order& order);
    void matchOrders(OrderPointer newOrder);

    void processLoop();

    OrderPool<Order> orderPool_;
    RingBuffer<OrderRequest> buffer_;

    std::thread workerThread_;

    std::unordered_map<Price, Quantity> bidLevels_;
    std::unordered_map<Price, Quantity> askLevels_;

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderPointer> orders_;

    size_t size_{0};
    bool verbose_{false};

    void printState() const;

};
