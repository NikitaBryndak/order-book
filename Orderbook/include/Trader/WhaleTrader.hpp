#pragma once

#include "Config.hpp"
#include "Trader.hpp"

class WhaleTrader : public Trader {
 public:
  WhaleTrader(uint32_t id, uint64_t cash, Orderbook& ob)
      : Trader(id, cash, Strategy::Whale, ob) {}

  void tick() override {

  }
};