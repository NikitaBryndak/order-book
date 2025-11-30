#include <gtest/gtest.h>
#include "Orderbook.hpp"
#include "Order.hpp"

TEST(OrderBookTests, SanityCheck) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(OrderBookTests, OrderBookEmptyByDefault) {
    Orderbook ob;
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, AddOrder) {
    Orderbook ob;
    Order order(1, 100, 10, Side::Buy);
    ob.addOrder(order);
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, CancelOrder) {
    Orderbook ob;
    Order order(1, 100, 10, Side::Buy);
    ob.addOrder(order);
    EXPECT_EQ(ob.size(), 1);

    ob.cancelOrder(1);
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, MatchFullFill) {
    Orderbook ob;
    // Sell 10 units at price 100
    Order sellOrder(1, 100, 10, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy 10 units at price 100
    Order buyOrder(2, 100, 10, Side::Buy);
    ob.addOrder(buyOrder);

    // Both should be filled and removed
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, MatchPartialFillBuyRemainder) {
    Orderbook ob;
    // Sell 5 units at price 100
    Order sellOrder(1, 100, 5, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy 10 units at price 100
    Order buyOrder(2, 100, 10, Side::Buy);
    ob.addOrder(buyOrder);

    // Sell order filled and removed. Buy order partially filled but remains.
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, MatchPartialFillSellRemainder) {
    Orderbook ob;
    // Sell 10 units at price 100
    Order sellOrder(1, 100, 10, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy 5 units at price 100
    Order buyOrder(2, 100, 5, Side::Buy);
    ob.addOrder(buyOrder);

    // Buy order filled and removed. Sell order partially filled but remains.
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, NoMatch) {
    Orderbook ob;
    // Sell at 101
    Order sellOrder(1, 101, 10, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy at 99
    Order buyOrder(2, 99, 10, Side::Buy);
    ob.addOrder(buyOrder);

    // No match, both remain
    EXPECT_EQ(ob.size(), 2);
}

TEST(OrderBookTests, MatchMultipleOrders) {
    Orderbook ob;
    // Sell 10 @ 100
    ob.addOrder(Order(1, 100, 10, Side::Sell));
    // Sell 10 @ 100
    ob.addOrder(Order(2, 100, 10, Side::Sell));

    // Buy 15 @ 100 (Should fill first sell completely, second sell partially)
    ob.addOrder(Order(3, 100, 15, Side::Buy));

    EXPECT_EQ(ob.size(), 1); // Only the second sell order remains (with 5 units)
}