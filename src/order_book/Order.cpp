#include "order_book/Order.h"

void Order::Fill(Quantity quantity)
{
    if (quantity > GetRemainingQuantity())
    {
        throw std::logic_error(
            "Order (" + std::to_string(GetOrderId()) + ") cannot be filled: insufficient quantity");
    }
    remainingQuantity_ -= quantity;
}

void Order::ToGoodTillCancel(Price price)
{
    if (GetOrderType() != OrderType::Market)
    {
        throw std::logic_error("Only market orders can be converted to good till cancel orders");
    }
    price_ = price;
    orderType_ = OrderType::GoodTillCancel;
}

bool Order::isFilled() const
{
    return GetRemainingQuantity() == 0;
}

OrderPointer OrderModify::ToOrderPointer(OrderType type)
{
    return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
}

std::ostream& operator<<(std::ostream& os, const Order& order)
{
    os << "Order{orderId=" << order.GetOrderId()
       << ", side=" << (order.GetSide() == Side::Buy ? "Buy" : "Sell")
       << ", price=" << order.GetPrice()
       << ", initialQuantity=" << order.GetInitialQuantity()
       << ", remainingQuantity=" << order.GetRemainingQuantity()
       << ", orderType=";
    switch (order.GetOrderType())
    {
        case OrderType::Market:
            os << "Market";
            break;
        case OrderType::GoodTillCancel:
            os << "GoodTillCancel";
            break;
        case OrderType::FillOrKill:
            os << "FillOrKill";
            break;
        case OrderType::FillAndKill:
            os << "FillAndKill";
            break;
    }
    os << "}";
    return os;
}