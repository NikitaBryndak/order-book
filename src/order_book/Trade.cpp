#include "order_book/Trade.h"

std::ostream &operator<<(std::ostream &os, const Trade &trade)
{
    os << "Trade{\n  BidTrade: " << trade.bidTrade_ << ",\n  AskTrade: " << trade.askTrade_ << "\n}";
    return os;
}