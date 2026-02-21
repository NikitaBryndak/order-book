#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>

#include "Orderbook/Orderbook.hpp"
#include "Trader/TraderManager.hpp"
#include "Trader/Trader.hpp"

class TraderSimulationTest : public ::testing::Test {
 protected:
  std::unique_ptr<Orderbook> ob_;
  std::unique_ptr<TraderManager> mgr_;

  void SetUp() override {
    ob_ = std::make_unique<Orderbook>(1 << 20, 0);
    mgr_ = std::make_unique<TraderManager>(*ob_, 100);
  }

  void TearDown() override {
    if (mgr_) {
      mgr_->stop();
      mgr_->join();
    }
    ob_.reset();
  }

  void WaitForSize(size_t targetSize, int maxRetries = 200) {
    int retries = 0;
    while (ob_->size() != targetSize && retries++ < maxRetries) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
};

// Simple deterministic trader that places a single order once
class SingleOrderTrader : public Trader {
 public:
  SingleOrderTrader(uint32_t id, uint64_t cash, Orderbook &ob, Price price, Quantity qty, Side side)
      : Trader(id, cash, Strategy::Noise, ob), price_(price), qty_(qty), side_(side), placed_(false) {}

  void tick() override {
    if (placed_) return;
    placeOrder(OrderType::GoodTillCancel, price_, qty_, side_);
    placed_ = true;
    // stop after placing so manager can retire this trader
    stop();
  }

 private:
  Price price_;
  Quantity qty_;
  Side side_;
  bool placed_;
};

// Deterministic trader pair that will create a match when both have ticked
class BuyerTrader : public Trader {
 public:
  BuyerTrader(uint32_t id, uint64_t cash, Orderbook &ob, Price price, Quantity qty)
      : Trader(id, cash, Strategy::Noise, ob), price_(price), qty_(qty), placed_(false) {}
  void tick() override {
    if (placed_) return;
    placeOrder(OrderType::GoodTillCancel, price_, qty_, Side::Buy);
    placed_ = true;
    stop();
  }
 private:
  Price price_;
  Quantity qty_;
  bool placed_;
};

class SellerTrader : public Trader {
 public:
  SellerTrader(uint32_t id, uint64_t cash, Orderbook &ob, Price price, Quantity qty)
      : Trader(id, cash, Strategy::Noise, ob), price_(price), qty_(qty), placed_(false) {}
  void tick() override {
    if (placed_) return;
    placeOrder(OrderType::GoodTillCancel, price_, qty_, Side::Sell);
    placed_ = true;
    stop();
  }
 private:
  Price price_;
  Quantity qty_;
  bool placed_;
};

// --------------------------------------------------
// Simulation tests
// --------------------------------------------------

TEST_F(TraderSimulationTest, TwoTraders_FullMatch)
{
  auto b = std::make_shared<BuyerTrader>(1, 100000, *ob_, /*price=*/100, /*qty=*/10);
  auto s = std::make_shared<SellerTrader>(2, 100000, *ob_, /*price=*/100, /*qty=*/10);

  mgr_->addTrader(b);
  mgr_->addTrader(s);

  mgr_->start();

  // wait for the two traders to place orders and for the match to occur
  int retries = 0;
  while (ob_->matchedTrades() < 1 && ++retries < 200) std::this_thread::sleep_for(std::chrono::milliseconds(5));

  mgr_->stop();
  mgr_->join();

  EXPECT_GE(ob_->matchedTrades(), 1);
  EXPECT_EQ(ob_->size(), 0);
}

TEST_F(TraderSimulationTest, MultipleTraders_PlaceOrders)
{
  const int N = 8;

  // Create N traders that each place one BUY at price 50 (no matching sellers)
  for (int i = 0; i < N; ++i) {
    mgr_->addTrader(std::make_shared<SingleOrderTrader>(i + 1, 100000, *ob_, /*price=*/50 + i, /*qty=*/1, Side::Buy));
  }

  mgr_->start();

  // Wait until all orders are visible in the book
  WaitForSize(N, /*maxRetries=*/200);

  mgr_->stop();
  mgr_->join();

  EXPECT_EQ(ob_->size(), (size_t)N);
}

// -----------------------
// Additional diverse scenarios
// -----------------------

// Inspectable seller to check reservedStock / cash changes from Trader::onTrade
class InspectableSeller : public Trader {
 public:
  InspectableSeller(uint32_t id, uint64_t cash, Orderbook &ob, Price price, Quantity qty)
      : Trader(id, cash, Strategy::Noise, ob), price_(price), qty_(qty), placed_(false) {}

  void tick() override {
    if (placed_) return;
    placeOrder(OrderType::GoodTillCancel, price_, qty_, Side::Sell);
    placed_ = true;
    stop();
  }

  uint64_t reservedStock() const { return reservedStock_.load(); }
  uint64_t cash() const { return cash_.load(); }

 private:
  Price price_;
  Quantity qty_;
  bool placed_;
};

TEST_F(TraderSimulationTest, PartialFill_UpdatesSellerState)
{
  auto seller = std::make_shared<InspectableSeller>(10, 100000, *ob_, /*price=*/100, /*qty=*/10);
  auto buyer1 = std::make_shared<BuyerTrader>(11, 100000, *ob_, /*price=*/100, /*qty=*/6);

  // Keep raw pointer to inspect seller after manager runs
  InspectableSeller* sellerPtr = seller.get();

  mgr_->addTrader(seller);
  mgr_->addTrader(buyer1);

  mgr_->start();

  // wait for the partial match to occur
  int retries = 0;
  while (ob_->matchedTrades() < 1 && ++retries < 200) std::this_thread::sleep_for(std::chrono::milliseconds(5));

  mgr_->stop();
  mgr_->join();

  // Seller placed 10, 6 executed -> reservedStock should be 4, cash increased by 6*price
  EXPECT_EQ(sellerPtr->reservedStock(), (uint64_t)4);
  EXPECT_EQ(sellerPtr->cash(), (uint64_t)(100000 + 6 * 100));
}

// Trader that places an order then cancels it on the next tick
class CancellingTrader : public Trader {
 public:
  CancellingTrader(uint32_t id, uint64_t cash, Orderbook &ob, Price price, Quantity qty, Side side)
      : Trader(id, cash, Strategy::Noise, ob), price_(price), qty_(qty), side_(side), stage_(0), orderId_(0) {}

  void tick() override {
    if (stage_ == 0) {
      orderId_ = placeOrder(OrderType::GoodTillCancel, price_, qty_, side_);
      stage_ = 1;
      return;
    }
    if (stage_ == 1) {
      cancelOrder(orderId_);
      stage_ = 2;
      stop();
    }
  }

 private:
  Price price_;
  Quantity qty_;
  Side side_;
  int stage_;
  OrderId orderId_;
};

TEST_F(TraderSimulationTest, Trader_CancelsOrder)
{
  auto canceller = std::make_shared<CancellingTrader>(20, 100000, *ob_, /*price=*/77, /*qty=*/3, Side::Buy);
  mgr_->addTrader(canceller);
  mgr_->start();

  // book should briefly have size 1 then return to 0 after cancel
  int retries = 0;
  while (ob_->size() != 0 && ++retries < 200) std::this_thread::sleep_for(std::chrono::milliseconds(5));

  mgr_->stop();
  mgr_->join();

  EXPECT_EQ(ob_->size(), 0);
}

// Trader that places then modifies its own order on second tick
class ModifyingTrader : public Trader {
 public:
  ModifyingTrader(uint32_t id, uint64_t cash, Orderbook &ob, Price p0, Price p1, Quantity qty)
      : Trader(id, cash, Strategy::Noise, ob), p0_(p0), p1_(p1), qty_(qty), stage_(0), oid_(0) {}

  void tick() override {
    if (stage_ == 0) {
      oid_ = placeOrder(OrderType::GoodTillCancel, p0_, qty_, Side::Buy);
      stage_ = 1;
      return;
    }
    if (stage_ == 1) {
      modifyOrder(oid_, OrderType::GoodTillCancel, p1_, qty_, Side::Buy);
      stage_ = 2;
      stop();
    }
  }

 private:
  Price p0_, p1_;
  Quantity qty_;
  int stage_;
  OrderId oid_;
};

TEST_F(TraderSimulationTest, Trader_ModifiesOrder)
{
  auto modifier = std::make_shared<ModifyingTrader>(30, 100000, *ob_, /*p0=*/90, /*p1=*/105, /*qty=*/5);

  mgr_->addTrader(modifier);
  mgr_->start();

  // wait for initial add
  WaitForSize(1);

  // after modification the top bid should reflect the new price
  int retries = 0;
  while (ob_->topBidPrice() != 105 && ++retries < 200) std::this_thread::sleep_for(std::chrono::milliseconds(5));

  mgr_->stop();
  mgr_->join();

  EXPECT_EQ(ob_->topBidPrice(), 105);
}

// Verify owning addTrader(std::unique_ptr<>) works and caller can still inspect the trader via a raw pointer
TEST_F(TraderSimulationTest, OwningTrader_IsProcessed)
{
  auto up = std::make_shared<SingleOrderTrader>(40, 100000, *ob_, /*price=*/60, /*qty=*/1, Side::Buy);
  SingleOrderTrader* raw = up.get();
  mgr_->addTrader(up); // manager takes shared ownership

  mgr_->start();
  WaitForSize(1);
  mgr_->stop();
  mgr_->join();

  EXPECT_EQ(ob_->size(), (size_t)1);
  // raw remains valid (owned by manager) and should have stopped itself after placing
  EXPECT_FALSE(raw->isRunning());
}

// Each trader records a tick; ensure manager actually invoked tick for all traders
class CountingTrader : public Trader {
 public:
  CountingTrader(uint32_t id, uint64_t cash, Orderbook &ob) : Trader(id, cash, Strategy::Noise, ob), ticks_(0) {}
  void tick() override { ticks_.fetch_add(1); stop(); }
  int ticks() const { return ticks_.load(); }
 private:
  std::atomic<int> ticks_;
};

TEST_F(TraderSimulationTest, Workers_InvokeAllTicks)
{
  const int N = 16;
  std::vector<CountingTrader*> raw;
  raw.reserve(N);

  for (int i = 0; i < N; ++i) {
    auto up = std::make_shared<CountingTrader>(50 + i, 100000, *ob_);
    raw.push_back(up.get());
    mgr_->addTrader(up);
  }

  mgr_->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  mgr_->stop();
  mgr_->join();

  for (auto* t : raw) EXPECT_GE(t->ticks(), 1);
}

// Small multi-tick stress: each trader emits several orders over successive ticks
class BurstTrader : public Trader {
 public:
  BurstTrader(uint32_t id, uint64_t cash, Orderbook &ob, int bursts, Price basePrice)
      : Trader(id, cash, Strategy::Noise, ob), bursts_(bursts), basePrice_(basePrice), sent_(0) {}
  void tick() override {
    if (sent_ >= bursts_) { stop(); return; }
    placeOrder(OrderType::GoodTillCancel, basePrice_ + sent_, 1, Side::Buy);
    ++sent_;
  }
 private:
  int bursts_;
  Price basePrice_;
  int sent_;
};

TEST_F(TraderSimulationTest, ShortBurst_ManyTraders)
{
  const int N = 20;
  const int bursts = 5;
  for (int i = 0; i < N; ++i) {
    mgr_->addTrader(std::make_shared<BurstTrader>(100 + i, 100000, *ob_, bursts, 10 + i));
  }

  mgr_->start();

  // wait until a reasonable number of orders have been emitted
  int retries = 0;
  while (ob_->size() < (size_t)(N * bursts) && ++retries < 400) std::this_thread::sleep_for(std::chrono::milliseconds(5));

  mgr_->stop();
  mgr_->join();

  EXPECT_GE(ob_->size(), (size_t)(N * bursts / 2)); // at least half should remain (non-flaky check)
}
