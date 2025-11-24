#include <iostream>

#include "order_book/Order.h"
#include "order_book/Orderbook.h"

int main()
{
    Orderbook orderbook;

    OrderId id = 1;
    Order x(OrderType::FillAndKill, id, Side::Buy, 100, 10.0);

    std::cout << x;

    return 0;
}