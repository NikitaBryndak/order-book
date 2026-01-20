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
    Order order(1, OrderType::GoodTillCancel, 100, 10, Side::Buy);
    ob.addOrder(order);
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, CancelOrder) {
    Orderbook ob;
    Order order(1, OrderType::GoodTillCancel, 100, 10, Side::Buy);
    ob.addOrder(order);
    EXPECT_EQ(ob.size(), 1);

    ob.cancelOrder(1);
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, MatchFullFill) {
    Orderbook ob;
    // Sell 10 units at price 100
    Order sellOrder(1, OrderType::GoodTillCancel, 100, 10, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy 10 units at price 100
    Order buyOrder(2, OrderType::GoodTillCancel, 100, 10, Side::Buy);
    ob.addOrder(buyOrder);

    // Both should be filled and removed
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, MatchPartialFillBuyRemainder) {
    Orderbook ob;
    // Sell 5 units at price 100
    Order sellOrder(1, OrderType::GoodTillCancel, 100, 5, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy 10 units at price 100
    Order buyOrder(2, OrderType::GoodTillCancel, 100, 10, Side::Buy);
    ob.addOrder(buyOrder);

    // Sell order filled and removed. Buy order partially filled but remains.
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, MatchPartialFillSellRemainder) {
    Orderbook ob;
    // Sell 10 units at price 100
    Order sellOrder(1, OrderType::GoodTillCancel, 100, 10, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy 5 units at price 100
    Order buyOrder(2, OrderType::GoodTillCancel, 100, 5, Side::Buy);
    ob.addOrder(buyOrder);

    // Buy order filled and removed. Sell order partially filled but remains.
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, NoMatch) {
    Orderbook ob;
    // Sell at 101
    Order sellOrder(1, OrderType::GoodTillCancel, 101, 10, Side::Sell);
    ob.addOrder(sellOrder);

    // Buy at 99
    Order buyOrder(2, OrderType::GoodTillCancel, 99, 10, Side::Buy);
    ob.addOrder(buyOrder);

    // No match, both remain
    EXPECT_EQ(ob.size(), 2);
}

TEST(OrderBookTests, MatchMultipleOrders) {
    Orderbook ob;
    // Sell 10 @ 100
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 100, 10, Side::Sell));
    // Sell 10 @ 100
    ob.addOrder(Order(2, OrderType::GoodTillCancel, 100, 10, Side::Sell));

    // Buy 15 @ 100 (Should fill first sell completely, second sell partially)
    ob.addOrder(Order(3, OrderType::GoodTillCancel, 100, 15, Side::Buy));

    EXPECT_EQ(ob.size(), 1); // Only the second sell order remains (with 5 units)
}

TEST(OrderBookTests, CancelOrderAtTop) {
    Orderbook ob;
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 100, 10, Side::Buy));
    ob.addOrder(Order(2, OrderType::GoodTillCancel, 100, 10, Side::Buy));

    ob.cancelOrder(1);

    // Sell 10 @ 100
    ob.addOrder(Order(3, OrderType::GoodTillCancel, 100, 10, Side::Sell));

    // Should match Order 2. Order 1 was cancelled.
    // If Order 1 was matched, Order 2 would remain.
    // If Order 2 was matched, Orderbook should be empty (Order 1 cancelled, Order 2 filled, Order 3 filled).

    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, MatchWithCancelledOrderInBetween) {
    Orderbook ob;
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 100, 10, Side::Buy));
    ob.addOrder(Order(2, OrderType::GoodTillCancel, 100, 10, Side::Buy));
    ob.addOrder(Order(3, OrderType::GoodTillCancel, 100, 10, Side::Buy));

    ob.cancelOrder(2);

    // Sell 30 @ 100.
    // Should fill Order 1 (10), skip Order 2, fill Order 3 (10).
    // Sell order has 10 remaining.
    ob.addOrder(Order(4, OrderType::GoodTillCancel, 100, 30, Side::Sell));

    EXPECT_EQ(ob.size(), 1);
}

// Tests for OrderType::FillAndKill

TEST(OrderBookTests, FillAndKill_NoMatch) {
    Orderbook ob;
    // Try to buy 10 @ 100, but book is empty
    ob.addOrder(Order(1, OrderType::FillAndKill, 100, 10, Side::Buy));

    // Order should not be added to the book
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, FillAndKill_FullMatch) {
    Orderbook ob;
    // Sell 10 @ 100
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 100, 10, Side::Sell));

    // Buy 10 @ 100 (FillAndKill)
    ob.addOrder(Order(2, OrderType::FillAndKill, 100, 10, Side::Buy));

    // Both should be gone
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, FillAndKill_PartialFill_RemainderKilled) {
    Orderbook ob;
    // Sell 5 @ 100
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 100, 5, Side::Sell));

    // Buy 10 @ 100 (FillAndKill)
    // Should fill 5, then kill the remaining 5
    ob.addOrder(Order(2, OrderType::FillAndKill, 100, 10, Side::Buy));

    // Sell order filled and removed. Buy order remainder killed.
    EXPECT_EQ(ob.size(), 0);
}

TEST(OrderBookTests, FillAndKill_DoesNotMatchWorsePrice) {
    Orderbook ob;
    // Sell 10 @ 105
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 105, 10, Side::Sell));

    // Buy 10 @ 100 (FillAndKill)
    // Price mismatch, no fill.
    ob.addOrder(Order(2, OrderType::FillAndKill, 100, 10, Side::Buy));

    // Sell order remains. Buy order killed.
    EXPECT_EQ(ob.size(), 1);
}

TEST(OrderBookTests, FillAndKill_PartialFill_OrderBookUpdates) {
    Orderbook ob;
    // Sell 10 @ 100
    ob.addOrder(Order(1, OrderType::GoodTillCancel, 100, 10, Side::Sell));
    // Sell 10 @ 101
    ob.addOrder(Order(2, OrderType::GoodTillCancel, 101, 10, Side::Sell));

    // Buy 15 @ 101 (FillAndKill)
    // Should match best price first: 10 @ 100.
    // Then match remaining 5 against: 10 @ 101.
    // Result: Order 1 filled/removed. Order 2 partial fill (5 remaining). Order 3 fully filled.
    ob.addOrder(Order(3, OrderType::FillAndKill, 101, 15, Side::Buy));

    EXPECT_EQ(ob.size(), 1);

    // Verify by clearing the remaining 5 of order 2
    ob.addOrder(Order(4, OrderType::FillAndKill, 101, 5, Side::Buy));
    EXPECT_EQ(ob.size(), 0);
}


TEST(OrderBookTests, PerformanceTest_SingleThread) {
    Orderbook ob;
    const int numOrders = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOrders; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price = 100 + (i % 10); // Prices around 100
        ob.addOrder(Order(i, OrderType::GoodTillCancel, price, 10, side));
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
                ob.addOrder(Order(id, OrderType::GoodTillCancel, price, 10, side));
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

TEST(OrderBookTests, Benchmark_DeepBook) {
    Orderbook orderbook;
    const int PRE_FILL_COUNT = 50000;
    const int TRADE_COUNT = 20000; // Number of orders to execute against the deep book

    std::cout << "1. Pre-filling order book with " << (PRE_FILL_COUNT * 2) << " resting orders..." << std::endl;

    // Fill Asks (Sells) at high prices (1000+)
    for(int i = 0; i < PRE_FILL_COUNT; i++) {
        orderbook.addOrder(Order(i, OrderType::GoodTillCancel, 1000 + (i % 500), 10, Side::Sell));
    }

    // Fill Bids (Buys) at low prices (0-500)
    for(int i = 0; i < PRE_FILL_COUNT; i++) {
        orderbook.addOrder(Order(i + PRE_FILL_COUNT, OrderType::GoodTillCancel, 100 + (i % 400), 10, Side::Buy));
    }

    std::cout << "   Current Book Size: " << orderbook.size() << " (Spread exists, no matches yet)" << std::endl;

    std::cout << "2. Benchmarking matching against deep book..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // Execute Buy orders that will match the waiting Sells (crossing the spread)
    // We buy at 2000, which guarantees a match against the Sells at 1000+
    for(int i = 0; i < TRADE_COUNT; i++) {
        // ID must be unique to avoid collisions in your logic if checking distinct IDs
        orderbook.addOrder(Order(1000000 + i, OrderType::FillAndKill, 2000, 10, Side::Buy));
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double ops = TRADE_COUNT / elapsed.count();
    double latencyUs = (elapsed.count() / TRADE_COUNT) * 1000000.0;

    std::cout << "=========================================" << std::endl;
    std::cout << "Deep Book Performance Results:" << std::endl;
    std::cout << "Orders Executed: " << TRADE_COUNT << std::endl;
    std::cout << "Total Time:      " << elapsed.count() << " s" << std::endl;
    std::cout << "Throughput:      " << ops << " OPS" << std::endl;
    std::cout << "Latency/Order:   " << latencyUs << " microseconds" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Sanity check: Size should have decreased by number of trades (since sells were removed)
    EXPECT_LT(orderbook.size(), (PRE_FILL_COUNT * 2));
}
