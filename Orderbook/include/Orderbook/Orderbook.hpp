#pragma once
#include <atomic>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <thread>

#ifdef OB_ENABLE_UI
#include <deque>
#include <mutex>
#endif

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

    explicit Orderbook(size_t maxOrders, int coreId = -1);

    Price topBidPrice() const;
    Price topAskPrice() const;

    void setTradeListener(TradeListener listener) { listener_ = listener; };

#ifdef OB_ENABLE_UI
    /// Thread-safe: returns the latest snapshot taken by the worker thread.
    OrderBookSnapshot getSnapshot() const {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        return snapshot_;
    }
#endif

    ~Orderbook();

private:
    void addOrder(const Order& order);
    void cancelOrder(const OrderId& orderId);
    void modifyOrder(const Order& order);
    void matchOrders(OrderPointer newOrder);

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

    TradeListener listener_;

    std::atomic<uint64_t> matchedTrades_{0};

#ifdef OB_ENABLE_UI
    /* ------------------------------ Snapshot -------------------------------- */
    mutable std::mutex snapshotMutex_;
    OrderBookSnapshot snapshot_;

    /* ------------------------------ Candle data ------------------------------ */
    Candlestick currentCandle_;
    std::deque<Candlestick> candleHistory_;
    inline void recordTradePrice(Price price, Quantity qty);
    void takeSnapshot();
#endif

};

/* ========  Inline UI-support implementations (pure C++, no Qt)  ============ */
#ifdef OB_ENABLE_UI
#include "Config.hpp"

inline void Orderbook::recordTradePrice(Price price, Quantity qty) {
  if (!currentCandle_.isValid()) {
    currentCandle_.open  = price;
    currentCandle_.close = price;
    currentCandle_.low   = price;
    currentCandle_.high  = price;
  }

  currentCandle_.close = price;
  if (price < currentCandle_.low)  currentCandle_.low  = price;
  if (price > currentCandle_.high) currentCandle_.high = price;
  currentCandle_.volume     += qty;
  currentCandle_.tradeCount += 1;

  if (currentCandle_.tradeCount >= (uint64_t)Config::candleTradesPerCandle) {
    candleHistory_.push_back(currentCandle_);
    if ((int)candleHistory_.size() > Config::candleMaxCandles)
      candleHistory_.pop_front();
    currentCandle_ = Candlestick{};
  }
}

inline void Orderbook::takeSnapshot() {
  OrderBookSnapshot snap;
  for (auto& [p, q] : bidLevels_) snap.bidLevels.push_back({p, q});
  std::sort(snap.bidLevels.begin(), snap.bidLevels.end(),
            [](auto& a, auto& b) { return a.first > b.first; });

  for (auto& [p, q] : askLevels_) snap.askLevels.push_back({p, q});
  std::sort(snap.askLevels.begin(), snap.askLevels.end());

  snap.candles = candleHistory_;
  if (currentCandle_.isValid()) snap.candles.push_back(currentCandle_);
  snap.topBid = topBidPrice();
  snap.topAsk = topAskPrice();
  snap.orderCount = size_;
  snap.matchCount = matchedTrades_.load();

  {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_ = std::move(snap);
  }
}
#endif  // OB_ENABLE_UI
