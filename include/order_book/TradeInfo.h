#pragma once

#include <iostream>

#include "Constants.h"

struct TradeInfo {
    OrderId orderId_;
    Price _price;
    Quantity quantity_;
};

std::ostream& operator<<(std::ostream& os, const TradeInfo& tradeInfo);
