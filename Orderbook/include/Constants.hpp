#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>


/* -------------------------------------------------------------------------- */
/*                                  Orderbook                                 */
/* -------------------------------------------------------------------------- */

class Order;
using OrderPointer = std::shared_ptr<Order>;
using Price = uint64_t;  // uint64_t is used to get maximum precision on
                         // floating point numbers
using Quantity = uint64_t;
using OrderId = uint64_t;

enum struct Side { Buy, Sell };

enum class OrderType {
  FillAndKill,
  GoodTillCancel,
};

enum struct RequestType {Add, Cancel, Modify, Stop, Snapshot};

struct Trade {
  OrderPointer bid;
  OrderPointer ask;
  Quantity qty;
};

using Trades = std::vector<Trade>;

enum struct AckType { Accepted, Rejected, Cancelled };

using TradeListener = std::function<void(Trade&)>;
using AckListener = std::function<void(OrderPointer&)>;

/* -------------------------------------------------------------------------- */
/*                          UI-only types (guarded)                           */
/* -------------------------------------------------------------------------- */
#ifdef OB_ENABLE_UI

#include <deque>
#include <utility>

struct Candlestick
{
  Price open{0};
  Price close{0};
  Price low{0};
  Price high{0};
  uint64_t volume{0};
  uint64_t tradeCount{0};

  bool isBullish() const { return close >= open; }
  bool isValid() const { return tradeCount > 0; }
};

struct OrderBookSnapshot {
  std::vector<std::pair<Price, Quantity>> bidLevels;
  std::vector<std::pair<Price, Quantity>> askLevels;
  std::deque<Candlestick> candles;
  Price topBid{0};
  Price topAsk{0};
  size_t orderCount{0};
  uint64_t matchCount{0};
};

#endif  // OB_ENABLE_UI

/* -------------------------------------------------------------------------- */
/*                                   Trader                                   */
/* -------------------------------------------------------------------------- */

enum struct Strategy { Noise, Momentum, Whale };
