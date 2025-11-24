#include <iostream>
#include <memory>

#include "order_book/Order.h"
#include "order_book/Orderbook.h"

int main()
{
    Orderbook orderbook;

    Order x1(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10);
    Order x2(OrderType::GoodTillCancel, 2, Side::Sell, 104, 40);
    Order x3(OrderType::GoodTillCancel, 3, Side::Sell, 101, 10);
    Order x4(OrderType::GoodTillCancel, 4, Side::Buy, 110, 20);

    orderbook.AddOrder(std::make_shared<Order>(x1));
    orderbook.AddOrder(std::make_shared<Order>(x2));
    orderbook.AddOrder(std::make_shared<Order>(x3));
    orderbook.AddOrder(std::make_shared<Order>(x4));

    return 0;
}