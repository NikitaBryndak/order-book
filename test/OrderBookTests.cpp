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
        // Initialize Orderbook with 1 Million capacity, Core 0, Verbose false
        ob_ = std::make_unique<Orderbook>(1 << 22, 0);
    }

    void TearDown() override
    {
        // Destructor acts as the stop signal and thread join
        ob_.reset();
    }

    // --- Helpers ---

    // Fixed: Added traderId (1) to Order constructor
    void AddBuy(OrderId id, Price price, Quantity qty, OrderType type = OrderType::GoodTillCancel)
    {
        // Order constructor is (orderId, owner, ...)
        Order order(id, 1, type, price, qty, Side::Buy);
        OrderRequest req{RequestType::Add, order};
        ob_->submitRequest(req);
    }

    void AddSell(OrderId id, Price price, Quantity qty, OrderType type = OrderType::GoodTillCancel)
    {
        // Order constructor is (orderId, owner, ...); owner 2 for sells
        Order order(id, 2, type, price, qty, Side::Sell); // Using Trader 2 for sells
        OrderRequest req{RequestType::Add, order};
        ob_->submitRequest(req);
    }

    void Cancel(OrderId id)
    {
        // Trader ID matches the AddBuy (1)
        // Order constructor is (orderId, owner, ...)
        Order order(id, 1, OrderType::GoodTillCancel, 0, 0, Side::Buy);
        OrderRequest req{RequestType::Cancel, order};
        ob_->submitRequest(req);
    }

    void Modify(OrderId id, OrderType type, Price price, Quantity qty, Side side)
    {
        // Order constructor is (orderId, owner, ...)
        Order order(id, 1, type, price, qty, side);
        OrderRequest req{RequestType::Modify, order};
        ob_->submitRequest(req);
    }

    // Helper to wait for the async worker to update the book size
    void WaitForSize(size_t targetSize)
    {
        int retries = 0;
        while (ob_->size() != targetSize && retries < 100)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
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
    // Resting sell order (Trader 2)
    AddSell(1, 100, 10);
    WaitForSize(1);

    // Aggressive buy order matches completely (Trader 1)
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(ob_->size(), 1);

    // Verification: It should now match a sell at 105
    AddSell(2, 105, 10);
    WaitForSize(0);
    EXPECT_EQ(ob_->size(), 0);
}

// ==========================================
// 1.b ADDITIONAL UNIT TESTS
// ==========================================

TEST_F(OrderBookTest, TopPrices_AfterAdds)
{
    AddBuy(1, 100, 10);
    AddBuy(2, 110, 10);
    AddSell(3, 120, 10);
    AddSell(4, 115, 10);

    WaitForSize(4);

    EXPECT_EQ(ob_->topBidPrice(), 110);
    EXPECT_EQ(ob_->topAskPrice(), 115);
}

TEST_F(OrderBookTest, TradeListener_SingleMatch)
{
    Trades trades;
    ob_->setTradeListener([&trades](Trade &t) { trades.push_back(t); });

    AddSell(1, 100, 10);
    WaitForSize(1);
    AddBuy(2, 100, 10);

    // wait for async match
    int retries = 0;
    while (trades.size() < 1 && retries++ < 100) std::this_thread::sleep_for(std::chrono::milliseconds(2));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].qty, 10);
    EXPECT_EQ(trades[0].bid->getPrice(), 100);
    EXPECT_EQ(trades[0].ask->getPrice(), 100);
    EXPECT_EQ(ob_->matchedTrades(), 1);
}

TEST_F(OrderBookTest, FillAndKill_NotResting)
{
    // Order constructor is (orderId, owner, ...)
    Order order(999, 1, OrderType::FillAndKill, 100, 10, Side::Buy);
    OrderRequest req{RequestType::Add, order};
    ob_->submitRequest(req);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(ob_->size(), 0);
    EXPECT_EQ(ob_->topBidPrice(), 0);
}

TEST_F(OrderBookTest, CancelNonexistent_NoCrash)
{
    AddBuy(1, 100, 10);
    WaitForSize(1);

    Cancel(9999); // no such order
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_EQ(ob_->size(), 1);
}

TEST_F(OrderBookTest, PriceTimePriority_FIFO)
{
    Trades trades;
    ob_->setTradeListener([&trades](Trade &t) { trades.push_back(t); });

    // Two resting asks at same price (IDs 1 then 2)
    AddSell(1, 100, 10);
    AddSell(2, 100, 10);
    WaitForSize(2);

    // Aggressive buy consumes first ask fully then partially second
    AddBuy(3, 100, 15);

    int retries = 0;
    while (trades.size() < 2 && retries++ < 200) std::this_thread::sleep_for(std::chrono::milliseconds(2));

    ASSERT_GE(trades.size(), 2);
    EXPECT_EQ(trades[0].ask->getOrderId(), 1);
    EXPECT_EQ(trades[1].ask->getOrderId(), 2);
}

TEST_F(OrderBookTest, ModifyOrder_ChangePrice_Reshuffles)
{
    AddBuy(1, 100, 10);
    AddBuy(2, 101, 10);
    WaitForSize(2);

    // Move order 1 to a better price
    Modify(1, OrderType::GoodTillCancel, 102, 10, Side::Buy);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(ob_->topBidPrice(), 102);
    EXPECT_EQ(ob_->size(), 2);
}

TEST_F(OrderBookTest, TopPrice_AfterCancel)
{
    AddBuy(1, 100, 10);
    AddBuy(2, 110, 10);
    WaitForSize(2);

    Cancel(2);
    WaitForSize(1);

    EXPECT_EQ(ob_->topBidPrice(), 100);
}

TEST_F(OrderBookTest, ModifyOrder_ChangeSide)
{
    AddBuy(1, 100, 10);
    WaitForSize(1);

    // Change side to Sell with same ID
    Modify(1, OrderType::GoodTillCancel, 100, 10, Side::Sell);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(ob_->topBidPrice(), 0);
    EXPECT_EQ(ob_->topAskPrice(), 100);
    EXPECT_EQ(ob_->size(), 1);
}

// ==========================================
// 2. HIGH PERFORMANCE BENCHMARKS
// ==========================================

TEST_F(OrderBookTest, Benchmark_OrderInsertion)
{
    const int numOrders = 10000000; // Reverted to 5M
    std::cout << "Starting Insertion Benchmark (" << numOrders << " orders)...\n";

    auto start = std::chrono::high_resolution_clock::now();

    {
        // Use a local Orderbook to measure full lifecycle (including thread drain)
        Orderbook benchOb(1 << 24); // 16 Million slots

        for (int i = 0; i < numOrders; ++i)
        {
            // TraderID 1, OrderID i
            // Order constructor is (orderId, owner, ...)
            Order order(i, 1, OrderType::GoodTillCancel, 100, 10, Side::Buy);
            OrderRequest req{RequestType::Add, order};
            benchOb.submitRequest(req);
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
    const int numOps = 50000000; // Ops per thread (Total 10M ops)

    std::cout << "Starting Multi-Threaded Benchmark (" << numThreads << " producers, " << numOps * numThreads << " total ops)...\n";

    auto start = std::chrono::high_resolution_clock::now();

    {
        Orderbook benchOb(1 << 26);
        std::vector<std::thread> producers;
        producers.reserve(numThreads);

        for (int t = 0; t < numThreads; ++t)
        {
            producers.emplace_back(
                [&benchOb, t, numOps]()
                {
                    // Random generators
                    std::mt19937 gen(12345 + t);
                    std::uniform_int_distribution<> qtyDist(1, 100);
                    std::uniform_int_distribution<> sideDist(0, 1);

                    // Partition IDs to avoid collision
                    OrderId nextOrderId = (OrderId)t * (OrderId)numOps * 2 + 1;

                    for (int i = 0; i < numOps; ++i)
                    {
                        // Simple 50/50 buy/sell mix
                        Side s = sideDist(gen) == 0 ? Side::Buy : Side::Sell;

                        // TraderID = t
                        // Order constructor is (orderId, owner, ...)
                        Order order(nextOrderId++, t, OrderType::GoodTillCancel, 100, qtyDist(gen), s);
                        OrderRequest req{RequestType::Add, order};

                        benchOb.submitRequest(req);
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

    long long totalOps = numOps * numThreads;
    std::cout << "Processed " << totalOps << " ops in " << diff.count() << " s\n";
    std::cout << "Throughput: " << (long long)(totalOps / diff.count()) << " ops/sec\n";
    std::cout << "Average Latency: " << (diff.count() / totalOps) * 1e6 << " us/order\n";
}