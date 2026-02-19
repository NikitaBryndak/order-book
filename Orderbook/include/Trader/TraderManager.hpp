#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>

#include "Trader/Trader.hpp"
#include "Orderbook/Orderbook.hpp"

class TraderManager {
 public:
  explicit TraderManager(Orderbook& ob, size_t sleepUs = 1000)
      : ob_(ob), running_(false), nextOrderId_(1), sleepUs_(sleepUs) {
    // forward trades from the orderbook to only the involved traders (O(1))
    ob_.setTradeListener([this](Trade& t) {
      if (t.bid) {
        auto it = tradersById_.find(t.bid->getOwner());
        if (it != tradersById_.end()) it->second.get()->onTrade(t);
      }
      if (t.ask) {
        auto it2 = tradersById_.find(t.ask->getOwner());
        if (it2 != tradersById_.end() && (!t.bid || t.ask->getOwner() != t.bid->getOwner())) it2->second.get()->onTrade(t);
      }
    });
  }

  ~TraderManager() {
    // unregister trade listener to avoid callbacks into a destructed manager
    ob_.setTradeListener(nullptr);
    stop();
    join();
  }

  void addTrader(std::shared_ptr<Trader> t) {
    t->setManager(this);
    traders_.push_back(t);
    tradersById_.emplace(t->getId(), std::move(t));
  }

  void start() {
    running_.store(true);

    size_t workerCount = std::max<size_t>(1, std::thread::hardware_concurrency());

    for (size_t w = 0; w < workerCount; ++w) {
      threads_.emplace_back([this, w, workerCount]() {
        while (running_.load()) {
          for (size_t i = w; i < traders_.size(); i += workerCount) {
            std::shared_ptr<Trader> t = traders_[i];
            if (!t) continue;
            if (!running_.load() || !t->isRunning()) break;
            t->tick();
          }
          std::this_thread::sleep_for(std::chrono::microseconds(sleepUs_));
        }
      });
    }
  }

  void stop() {
    running_.store(false);
    for (auto t : traders_) t->stop();
  }

  void join() {
    for (auto& th : threads_) if (th.joinable()) th.join();
    threads_.clear();
  }

  OrderId nextOrderId() { return nextOrderId_.fetch_add(1); }

 private:
  Orderbook& ob_;
  std::vector<std::shared_ptr<Trader>> traders_;
  std::unordered_map<uint32_t, std::shared_ptr<Trader>> tradersById_;
  std::vector<std::thread> threads_;
  std::atomic<bool> running_;
  std::atomic<OrderId> nextOrderId_;
  size_t sleepUs_;
};