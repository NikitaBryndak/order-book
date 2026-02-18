#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>

#include "Trader/Trader.hpp"
#include "Orderbook/Orderbook.hpp"

class TraderManager {
 public:
  explicit TraderManager(Orderbook& ob, size_t sleepUs = 1000)
      : ob_(ob), running_(false), nextOrderId_(1), sleepUs_(sleepUs) {
    // forward trades from the orderbook to traders
    ob_.setTradeListener([this](Trade& t) {
      for (auto* tr : traders_) tr->onTrade(t);
    });
  }

  ~TraderManager() {
    // unregister trade listener to avoid callbacks into a destructed manager
    ob_.setTradeListener(nullptr);
    stop();
    join();
  }

  void addTrader(std::unique_ptr<Trader> t) {
    t->setManager(this);
    tradersOwned_.push_back(std::move(t));
    traders_.push_back(tradersOwned_.back().get());
  }

  // non-owning add
  void addTrader(Trader* t) {
    t->setManager(this);
    traders_.push_back(t);
  }

  void start() {
    running_.store(true);
    for (auto* t : traders_) {
      threads_.emplace_back([this, t]() {
        while (running_.load() && t->isRunning()) {
          t->tick();
          std::this_thread::sleep_for(std::chrono::microseconds(sleepUs_));
        }
      });
    }
  }

  void stop() {
    running_.store(false);
    for (auto* t : traders_) t->stop();
  }

  void join() {
    for (auto& th : threads_) if (th.joinable()) th.join();
    threads_.clear();
  }

  OrderId nextOrderId() { return nextOrderId_.fetch_add(1); }

 private:
  Orderbook& ob_;
  std::vector<Trader*> traders_;
  std::vector<std::unique_ptr<Trader>> tradersOwned_;
  std::vector<std::thread> threads_;
  std::atomic<bool> running_;
  std::atomic<OrderId> nextOrderId_;
  size_t sleepUs_;
};