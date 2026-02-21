#pragma once

#include "Trader.hpp"
#include "Config.hpp"
#include <random>

class NoiseTrader : public Trader {
 public:
  NoiseTrader(uint32_t id, uint64_t cash, Orderbook& ob)
      : Trader(id, cash, Strategy::Noise, ob) {}

  void tick() override {
    static thread_local std::mt19937_64 gen(std::random_device{}());
    static thread_local std::uniform_int_distribution<int> actionDist(0, 99);
    static thread_local std::uniform_int_distribution<int> sideDist(0, 100);
    static thread_local std::uniform_int_distribution<int> offsetDist(-Config::nPSpread, Config::nPSpread);
    static thread_local std::uniform_int_distribution<Quantity> qtyDist(1, Config::nQSpread);

    int act = actionDist(gen);

    if (!orders_.empty() && act < 5) {
      // use modulo instead of constructing a distribution each tick
      size_t idx = gen() % orders_.size();
      cancelOrder(orders_[idx]);
      return;
    }

    if (orders_.size() < (size_t)Config::maxOrdersPerTrader && act < 50) {
      Side s = (sideDist(gen) < Config::makerP) ? Side::Buy : Side::Sell;

      Price base = 100;
      Price tb = ob_.topBidPrice();
      Price ta = ob_.topAskPrice();
      if (tb != 0 && ta != 0) base = (tb + ta) / 2;

      int offset = offsetDist(gen);
      Price price = (offset < 0 && base <= (Price)(-offset)) ? 1 : base + offset;
      if (price == 0) price = 1;

      Quantity qty = qtyDist(gen);

      placeOrder(OrderType::GoodTillCancel, price, qty, s);
    }
  }
};
