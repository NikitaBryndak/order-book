#include <ostream>
#include "Orderbook/Order.hpp"

std::ostream& operator<<(std::ostream &out, const Order &order)
{
    out << "Order {";
    out << " OrderId=" << order.getOrderId();
    out << " Price=" << order.getPrice();
    out << " InitialQuantity=" << order.getInitialQuantity();
    out << " RemainingQuantity=" << order.getQuantity();
    out << " Side=" << (order.getSide() == Side::Buy ? "Buy" : "Sell");
    out << " }";
    out << '\n';
    return out;
}