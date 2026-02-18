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
    uint64_t matchedTrades() const { return matchedTrades_.load(); };

    explicit Orderbook(size_t maxOrders, int coreId = -1, bool verbose = false);

    Price topBidPrice() const;
    Price topAskPrice() const;

    // void setAckListener(AckListener listener) { ackListener_ = listener; };
    void setTradeListener(TradeListener listener) { listener_ = listener; };

    ~Orderbook();

private:
    void addOrder(const Order& order);
    void cancelOrder(const OrderId& orderId);
    void modifyOrder(const Order& order);
    void matchOrders(OrderPointer newOrder);

    // void sendAck(OrderPointer&);
    inline void onMatch(const OrderPointer& b, const OrderPointer& a, Quantity& qty);

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

    TradeListener listener_;
    // AckListener ackListener_;

    std::atomic<uint64_t> matchedTrades_{0};

    void printState() const;

};
