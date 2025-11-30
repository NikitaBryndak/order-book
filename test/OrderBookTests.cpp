#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <iostream>
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

TEST(OrderBookTests, CancelOrderAtTop) {
    Orderbook ob;
    ob.addOrder(Order(1, 100, 10, Side::Buy));
    ob.addOrder(Order(2, 100, 10, Side::Buy));

    ob.cancelOrder(1);

    // Sell 10 @ 100
    ob.addOrder(Order(3, 100, 10, Side::Sell));

    // Should match Order 2. Order 1 was cancelled.
    // If Order 1 was matched, Order 2 would remain.
    // If Order 2 was matched, Orderbook should be empty (Order 1 cancelled, Order 2 filled, Order 3 filled).

    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, MatchWithCancelledOrderInBetween) {
    Orderbook ob;
    ob.addOrder(Order(1, 100, 10, Side::Buy));
    ob.addOrder(Order(2, 100, 10, Side::Buy));
    ob.addOrder(Order(3, 100, 10, Side::Buy));

    ob.cancelOrder(2);

    // Sell 30 @ 100.
    // Should fill Order 1 (10), skip Order 2, fill Order 3 (10).
    // Sell order has 10 remaining.
    ob.addOrder(Order(4, 100, 30, Side::Sell));

    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, PerformanceTest_SingleThread) {
    Orderbook ob;
    const int numOrders = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOrders; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price = 100 + (i % 10); // Prices around 100
        ob.addOrder(Order(i, price, 10, side));
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double ops = numOrders / elapsed.count();
    std::cout << "Single Thread Performance: " << ops << " OPS" << std::endl;
}

TEST(OrderBookTests, PerformanceTest_MultiThread) {
    Orderbook ob;
    const int numOrders = 100000;
    const int numThreads = 4;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&ob, t, numThreads, numOrders]() {
            int ordersPerThread = numOrders / numThreads;
            int startId = t * ordersPerThread;
            for (int i = 0; i < ordersPerThread; ++i) {
                int id = startId + i;
                Side side = (id % 2 == 0) ? Side::Buy : Side::Sell;
                Price price = 100 + (id % 10);
                ob.addOrder(Order(id, price, 10, side));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double ops = numOrders / elapsed.count();
    std::cout << "Multi Thread (" << numThreads << " threads) Performance: " << ops << " OPS" << std::endl;
}
