#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <memory>

#include "Orderbook/Order.hpp"
#include "Orderbook/Orderbook.hpp"

class OrderBookTest : public ::testing::Test
{
protected:
    std::unique_ptr<Orderbook> ob_;

    void SetUp() override
    {
        // Initialize Orderbook with 1 Million capacity
        ob_ = std::make_unique<Orderbook>(1 << 20, 0, false);
    }

    void TearDown() override
    {
        // Destructor acts as the stop signal and thread join
        ob_.reset();
    }

    // --- Helpers ---

    void AddBuy(OrderId id, Price price, Quantity qty, OrderType type = OrderType::GoodTillCancel)
    {
        OrderRequest order({RequestType::Add, Order(id, type, price, qty, Side::Buy)});
        ob_->submitRequest(order);
    }

    void AddSell(OrderId id, Price price, Quantity qty, OrderType type = OrderType::GoodTillCancel)
    {
        OrderRequest order({RequestType::Add, Order(id, type, price, qty, Side::Sell)});
        ob_->submitRequest(order);
    }

    void Cancel(OrderId id)
    {
        // Dummy order just to carry the ID
        OrderRequest order({RequestType::Cancel, Order(id, OrderType::GoodTillCancel, 0, 0, Side::Buy)});
        ob_->submitRequest(order);
    }

    void Modify(OrderId id, OrderType type, Price price, Quantity qty, Side side)
    {
        OrderRequest order({RequestType::Modify, Order(id, type, price, qty, side)});
        ob_->submitRequest(order);
    }

    // Helper to wait for the async worker to update the book size
    void WaitForSize(size_t targetSize)
    {
        int retries = 0;
        while (ob_->size() != targetSize && retries < 100)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            retries++;
        }
        // If we timeout, the EXPECT_EQ in the test will fail and show us the error
    }
};

// ==========================================
// 1. FUNCTIONAL CORRECTNESS TESTS
// ==========================================

TEST_F(OrderBookTest, InitialStateIsEmpty)
{
    EXPECT_EQ(ob_->size(), 0);
}

TEST_F(OrderBookTest, AddOrderValidation)
{
    AddBuy(1, 100, 10);
    WaitForSize(1);
    EXPECT_EQ(ob_->size(), 1);

    AddSell(2, 110, 5);
    WaitForSize(2);
    EXPECT_EQ(ob_->size(), 2);
}

TEST_F(OrderBookTest, CancelOrderValidation)
{
    AddBuy(1, 100, 10);
    WaitForSize(1);

    Cancel(1);
    WaitForSize(0);
    EXPECT_EQ(ob_->size(), 0);
}

TEST_F(OrderBookTest, FullMatchExecution)
{
    // Resting sell order
    AddSell(1, 100, 10);
    WaitForSize(1);

    // Aggressive buy order matches completely
    AddBuy(2, 100, 10);
    WaitForSize(0);

    EXPECT_EQ(ob_->size(), 0);
}

TEST_F(OrderBookTest, PartialMatch_RestingRemains)
{
    // Provide 10 liquidity
    AddSell(1, 100, 10);
    WaitForSize(1);

    // Consume 5 liquidity
    AddBuy(2, 100, 5);

    // Give worker time to process match
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // Sell order should remain with 5 qty (Size 1)
    EXPECT_EQ(ob_->size(), 1);
}

TEST_F(OrderBookTest, ModifyOrder)
{
    AddBuy(1, 100, 10);
    WaitForSize(1);

    // Modify: Cancel 1, Add New at 105
    Modify(1, OrderType::GoodTillCancel, 105, 10, Side::Buy);

    // Wait for process (Cancel -> Add)
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_EQ(ob_->size(), 1);

    // Verification: It should now match a sell at 105
    AddSell(2, 105, 10);
    WaitForSize(0);
    EXPECT_EQ(ob_->size(), 0);
}

// ==========================================
// 2. HIGH PERFORMANCE BENCHMARKS
// ==========================================

TEST_F(OrderBookTest, Benchmark_OrderInsertion)
{
    const int numOrders = 5000000;
    std::cout << "Starting Insertion Benchmark (" << numOrders << " orders)...\n";

    auto start = std::chrono::high_resolution_clock::now();

    {
        // Use a local Orderbook to measure full lifecycle (including thread drain)
        Orderbook benchOb(1 << 24); // 8 Million slots (power of 2)

        for (int i = 0; i < numOrders; ++i)
        {
            OrderRequest order({RequestType::Add, Order(i, OrderType::GoodTillCancel, 100, 10, Side::Buy)});
            benchOb.submitRequest(order);
        }

        // Destructor called here: sends STOP, waits for thread to finish processing
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Inserted " << numOrders << " orders in " << diff.count() << " s\n";
    std::cout << "Throughput: " << (long long)(numOrders / diff.count()) << " ops/sec\n";
    std::cout << "Average Latency: " << (diff.count() / numOrders) * 1e6 << " us/order\n";
}

TEST_F(OrderBookTest, Benchmark_RealWorldScenario)
{
    const int numThreads = 10;
    const int numOps = 100000000;
    const int opsPerThread = numOps / numThreads;

    std::cout << "Starting Multi-Threaded Benchmark (" << numThreads << " producers, " << numOps << " ops)...\n";

    auto start = std::chrono::high_resolution_clock::now();

    {
        Orderbook benchOb(1 << 26);
        std::vector<std::thread> producers;
        producers.reserve(numThreads);

        for (int t = 0; t < numThreads; ++t)
        {
            producers.emplace_back(
                [&benchOb, t, opsPerThread]()
                {
                    // Random generators
                    std::mt19937 gen(12345 + t);
                    std::uniform_int_distribution<> actionDist(1, 100);
                    std::uniform_int_distribution<> qtyDist(1, 100);
                    std::uniform_int_distribution<> sideDist(0, 1);

                    // Partition IDs
                    OrderId nextOrderId = (OrderId)t * (OrderId)opsPerThread * 2 + 1;

                    for (int i = 0; i < opsPerThread; ++i)
                    {
                        int action = actionDist(gen);

                        // Simple 50/50 buy/sell mix for stress testing
                        Side s = sideDist(gen) == 0 ? Side::Buy : Side::Sell;
                        OrderRequest order({RequestType::Add, Order(nextOrderId++, OrderType::GoodTillCancel, 100, qtyDist(gen), s)});
                        benchOb.submitRequest(order);
                    }
                });
        }

        for (auto& t : producers)
        {
            if (t.joinable()) t.join();
        }

        // BenchOb destructor handles the drain
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Processed " << (opsPerThread * numThreads) << " ops in " << diff.count() << " s\n";
    std::cout << "Throughput: " << (long long)((opsPerThread * numThreads) / diff.count()) << " ops/sec\n";
    std::cout << "Average Latency: " << (diff.count() / numOps) * 1e6 << " us/order\n";
}