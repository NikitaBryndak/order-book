#include "order_book/TradeInfo.h"

std::ostream& operator<<(std::ostream& os, const TradeInfo& tradeInfo)
{
    os << "TradeInfo{orderId=" << tradeInfo.orderId_
       << ", price=" << tradeInfo._price
       << ", quantity=" << tradeInfo.quantity_ << "}";
    return os;
}
