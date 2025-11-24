#pragma once

#include <cstdint>
#include <limits>
#include <vector>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using OrderIds = std::vector<OrderId>;

constexpr Price InvalidPrice = std::numeric_limits<Price>::min();
enum struct Side { Buy, Sell };
