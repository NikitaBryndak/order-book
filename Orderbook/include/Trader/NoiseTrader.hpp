#pragma once

#include "Trader.hpp"
#include <random>

class NoiseTrader : public Trader {
 public:
  NoiseTrader(uint32_t id, uint64_t cash, Orderbook& ob)
      : Trader(id, cash, Strategy::Noise, ob) {}

  void tick() override {
    static thread_local std::mt19937_64 gen(std::random_device{}());
    std::uniform_int_distribution<int> action(0, 99);

    int act = action(gen);

    if (!orders_.empty() && act < 5) {
      std::uniform_int_distribution<size_t> idx(0, orders_.size() - 1);
      cancelOrder(orders_[idx(gen)]);
      return;
    }

    if (orders_.size() < (size_t)Config::maxOrdersPerTrader && act < 50) {
      Side s = (std::uniform_int_distribution<int>(0, 1)(gen) == 0) ? Side::Buy : Side::Sell;

      Price base = 100;
      Price tb = ob_.topBidPrice();
      Price ta = ob_.topAskPrice();
      if (tb != 0 && ta != 0) base = (tb + ta) / 2;

      int offset = std::uniform_int_distribution<int>(-Config::nPSpread, Config::nPSpread)(gen);
      Price price = (offset < 0 && base <= (Price)(-offset)) ? 1 : base + offset;
      if (price == 0) price = 1;

      Quantity qty = std::uniform_int_distribution<Quantity>(1, Config::nQSpread)(gen);

      placeOrder(OrderType::GoodTillCancel, price, qty, s);
    }
  }
};

