#include "Orderbook.hpp"
#include "Order.hpp"
#include "Constants.hpp"

#include <iostream>

int main(){
    Orderbook orderbook;

    Order x(1,100,10,Side::Buy);
    Order y(2,99,5,Side::Sell);

    orderbook.addOrder(x);
    orderbook.addOrder(y);

    std::cout << orderbook.size();

    return 0;
}