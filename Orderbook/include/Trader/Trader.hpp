#pragma once
#include <algorithm>
#include <atomic>
#include <deque>
#include <vector>

#include "Config.hpp"
#include "Constants.hpp"
#include "Orderbook/Orderbook.hpp"

class TraderManager; // forward

class Trader {
 public:
  Trader(uint32_t id, uint64_t cash, Strategy st, Orderbook& ob)
      : traderId_(id),
        manager_(nullptr),
        isRunning_(true),
        cash_(cash),
        protfolioSize_(cash),
        strategy_(st),
        ob_(ob) {}

  virtual ~Trader() {}

  /* ----------------------------- Getters & Setters ------------------- */
  Strategy getStrategy() const { return strategy_; }
  uint32_t getId() const { return traderId_; }

  virtual void tick() = 0;

  void onTrade(Trade& t) {
    if (t.bid->getOwner() == traderId_) {
      stock_ += t.qty;
      reservedCash_ -= t.ask->getPrice() * t.qty;
    } else if (t.ask->getOwner() == traderId_) {
      reservedStock_ -= t.qty;
      cash_ += t.bid->getPrice() * t.qty;
    }
  }

  void setManager(TraderManager* m);
  void stop();
  bool isRunning() const;

 protected:
  // Convenience helpers for derived traders
  OrderId placeOrder(OrderType type, Price price, Quantity qty, Side side);
  void cancelOrder(OrderId id);
  void modifyOrder(OrderId id, OrderType type, Price price, Quantity qty, Side side);

 protected:
  uint32_t traderId_;
  TraderManager* manager_;
  std::atomic<bool> isRunning_;

  std::atomic<uint64_t> cash_;
  std::atomic<uint64_t> reservedCash_{0};
  std::atomic<uint64_t> stock_{0};
  std::atomic<uint64_t> reservedStock_{0};

  uint32_t protfolioSize_;
  Strategy strategy_;
  Orderbook& ob_;

  std::deque<OrderId> orders_;
};