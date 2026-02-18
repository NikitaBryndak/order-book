#pragma once
#include <atomic>
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

enum struct RequestType { Add, Cancel, Modify, Stop, Print };

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
/*                                   Trader                                   */
/* -------------------------------------------------------------------------- */

enum struct Strategy { Noise, Momentum, Whale };
