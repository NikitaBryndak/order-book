#pragma once
#include <cstdint>

/* -------------------------------------------------------------------------- */
/*                                  Orderbook                                 */
/* -------------------------------------------------------------------------- */

using Price = uint64_t;  // uint64_t is used to get maximum precision on
                         // floating point numbers
using Quantity = uint64_t;
using OrderId = uint64_t;

enum struct Side { Buy, Sell };

enum struct RequestType { Add, Cancel, Modify, Stop };

/* -------------------------------------------------------------------------- */
/*                                   Trader                                   */
/* -------------------------------------------------------------------------- */

enum struct Strategy { Noise, Momentum, Whale };
