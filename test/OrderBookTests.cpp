#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "Order.hpp"
#include "Orderbook.hpp"
#include "RingBuffer.hpp"

class OrderBookTest : public ::testing::Test
{
protected:
    Orderbook ob;
    // Capacity 2^20 (~1 Million) to avoid blocking producers during benchmarks
    RingBuffer<OrderRequest> queue{1 << 20};
    std::thread consumerThread;

    // Sync mechanisms
    std::mutex syncMutex;
    std::condition_variable syncCv;
    bool syncDone = false;

    const OrderId STOP_ID = std::numeric_limits<OrderId>::max();
    const OrderId SYNC_ID = std::numeric_limits<OrderId>::max() - 1;

    void SetUp() override
    {
        consumerThread = std::thread(
            [this]()
            {
                while (true)
                {
                    try {
                        // This now SPINS (burns 100% CPU) waiting for data
                        // This eliminates the "Spikiness" of OS context switches
                        OrderRequest req = queue.pop();

                        OrderId id = req.order.getOrderId();
                        if (id == STOP_ID)
                            break;

                        if (id == SYNC_ID)
                        {
                            {
                                std::lock_guard<std::mutex> lock(syncMutex);
                                syncDone = true;
                            }
                            syncCv.notify_all();
                            continue;
                        }
                        ob.processRequest(req);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Consumer Ex: " << e.what() << std::endl;
                        // Don't rethrow to avoid crash, just log and break?
                        // If we break, sync might hang.
                        // Best to fail test.
                        std::terminate();
                    }
                    catch (const char* e) {
                         std::cerr << "Consumer Ex: " << e << std::endl;
                         std::terminate();
                    }
                    catch (...) {
                         std::cerr << "Consumer Unknown Ex" << std::endl;
                         std::terminate();
                    }
                }
            });
    }

    void TearDown() override
    {
        // Send stop signal
        Order stopOrder(STOP_ID, OrderType::GoodTillCancel, 0, 0, Side::Buy);
        OrderRequest req;
        req.type = RequestType::Add; // Type doesn't matter for logic flow here, just ID
        req.order = stopOrder;

        queue.push(std::move(req));

        if (consumerThread.joinable())
        {
            consumerThread.join();
        }
    }

    // Waits until the Consumer has processed everything currently in the queue
    void Sync()
    {
        {
            std::lock_guard<std::mutex> lock(syncMutex);
            syncDone = false;
        }

        Order syncOrder(SYNC_ID, OrderType::GoodTillCancel, 0, 0, Side::Buy);
        OrderRequest req;
        req.type = RequestType::Add;
        req.order = syncOrder;

        queue.push(std::move(req));

        std::unique_lock<std::mutex> lock(syncMutex);
        syncCv.wait(lock, [this]() { return syncDone; });
    }

    // --- Helpers for Functional Tests (Synchronous) ---
    // These calls wait for the engine, so they are slow but safe for assertions.

    void Add(OrderId id, OrderType type, Price price, Quantity qty, Side side)
    {
        OrderRequest req;
        req.type = RequestType::Add;
        req.order = Order(id, type, price, qty, side);
        queue.push(std::move(req));
        Sync();
    }

    void AddBuy(OrderId id, Price price, Quantity qty, OrderType type = OrderType::GoodTillCancel)
    {
        Add(id, type, price, qty, Side::Buy);
    }

    void AddSell(OrderId id, Price price, Quantity qty, OrderType type = OrderType::GoodTillCancel)
    {
        Add(id, type, price, qty, Side::Sell);
    }

    void Cancel(OrderId id)
    {
        OrderRequest req;
        req.type = RequestType::Cancel;
        // Dummy order object just to carry the ID
        req.order = Order(id, OrderType::GoodTillCancel, 0, 0, Side::Buy);
        queue.push(std::move(req));
        Sync();
    }

    void Modify(OrderId id, OrderType type, Price price, Quantity qty, Side side)
    {
        OrderRequest req;
        req.type = RequestType::Modify;
        req.order = Order(id, type, price, qty, side);
        queue.push(std::move(req));
        Sync();
    }
};

// ==========================================
// 1. FUNCTIONAL CORRECTNESS TESTS
// ==========================================

TEST_F(OrderBookTest, InitialStateIsEmpty) { EXPECT_EQ(ob.size(), 0); }

TEST_F(OrderBookTest, AddOrderValidation)
{
    AddBuy(1, 100, 10);
    EXPECT_EQ(ob.size(), 1);

    AddSell(2, 110, 5);
    EXPECT_EQ(ob.size(), 2);
}

TEST_F(OrderBookTest, CancelOrderValidation)
{
    AddBuy(1, 100, 10);
    EXPECT_EQ(ob.size(), 1);

    Cancel(1);
    EXPECT_EQ(ob.size(), 0);
}

TEST_F(OrderBookTest, CancelNonExistentOrder)
{
    Cancel(999);
    EXPECT_EQ(ob.size(), 0);

    AddBuy(1, 100, 10);
    Cancel(999);
    EXPECT_EQ(ob.size(), 1);
}

TEST_F(OrderBookTest, FullMatchExecution)
{
    // Resting sell order
    AddSell(1, 100, 10);

    // Aggressive buy order matches completely
    AddBuy(2, 100, 10);

    EXPECT_EQ(ob.size(), 0);
}

TEST_F(OrderBookTest, PartialMatch_RestingRemains)
{
    // Provide 10 liquidity
    AddSell(1, 100, 10);

    // Consume 5 liquidity
    AddBuy(2, 100, 5);

    // Sell order should remain with 5 qty
    EXPECT_EQ(ob.size(), 1);

    // Clear the rest
    AddBuy(3, 100, 5);
    EXPECT_EQ(ob.size(), 0);
}

TEST_F(OrderBookTest, PartialMatch_AggressiveRemains)
{
    // Provide 5 liquidity
    AddSell(1, 100, 5);

    // Try to consume 10. 5 Matched, 5 remaining placed in book.
    AddBuy(2, 100, 10);

    EXPECT_EQ(ob.size(), 1); // Order 2 remains
}

TEST_F(OrderBookTest, PriceImprovement_BuyMatchesLowerSell)
{
    // Seller willing to sell at 90
    AddSell(1, 90, 10);

    // Buyer willing to buy at 100. Should match at 90.
    AddBuy(2, 100, 10);

    EXPECT_EQ(ob.size(), 0);
}

TEST_F(OrderBookTest, ModifyOrder)
{
    AddBuy(1, 100, 10);
    Modify(1, OrderType::GoodTillCancel, 105, 10, Side::Buy);

    // Verification: It should now match a sell at 105
    AddSell(2, 105, 10);
    EXPECT_EQ(ob.size(), 0);
}

// --- OrderType::FillAndKill Tests ---

TEST_F(OrderBookTest, FillAndKill_NoLiquidity)
{
    AddBuy(1, 100, 10, OrderType::FillAndKill);
    EXPECT_EQ(ob.size(), 0); // Should be killed immediately
}

TEST_F(OrderBookTest, FillAndKill_PriceMismatch)
{
    AddSell(1, 110, 10);
    AddBuy(2, 100, 10, OrderType::FillAndKill);
    EXPECT_EQ(ob.size(), 1); // Sell remains, Buy killed
}

TEST_F(OrderBookTest, FillAndKill_PartialFill_KillRemainder)
{
    AddSell(1, 100, 5);

    // Wants 10, gets 5, remaining 5 killed
    AddBuy(2, 100, 10, OrderType::FillAndKill);

    EXPECT_EQ(ob.size(), 0);
}

// --- Lazy Deletion / Ghost Order Tests ---

TEST_F(OrderBookTest, LazyCleanup_TopLevelCancel)
{
    AddSell(1, 100, 10);
    AddSell(2, 100, 10);

    Cancel(1);
    EXPECT_EQ(ob.size(), 1);

    // Incoming buy should traverse the "ghost" of Order 1 and match Order 2
    AddBuy(3, 100, 10);
    EXPECT_EQ(ob.size(), 0);
}

// ==========================================
// 2. HIGH PERFORMANCE BENCHMARKS
// ==========================================
// Note: These tests bypass the 'Add' helper to avoid Sync() overhead per order.

TEST_F(OrderBookTest, Benchmark_OrderInsertion)
{
    const int numOrders = 5000000;
    std::cout << "Starting Insertion Benchmark (" << numOrders << " orders)...\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOrders; ++i)
    {
        OrderRequest req;
        req.type = RequestType::Add;
        req.order = Order(i, OrderType::GoodTillCancel, 100 - (i % 10), 10, Side::Buy);
        queue.push(std::move(req));
    }

    // Wait for engine to catch up at the END
    Sync();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Inserted " << numOrders << " orders in " << diff.count() << " s\n";
    std::cout << "Throughput: " << (long long)(numOrders / diff.count()) << " ops/sec\n";
    std::cout << "Average Latency: " << (diff.count() / numOrders) * 1e6 << " us/order\n";
}

TEST_F(OrderBookTest, Benchmark_OrderMatching)
{
    const int numOrders = 2500000; // 2.5M Buys + 2.5M Sells = 5M Ops

    // 1. Pre-fill Sells (Async)
    for (int i = 0; i < numOrders; ++i)
    {
        OrderRequest req;
        req.type = RequestType::Add;
        req.order = Order(i, OrderType::GoodTillCancel, 100, 10, Side::Sell);
        queue.push(std::move(req));
    }
    Sync(); // Ensure book is full

    std::cout << "Starting Matching Benchmark (" << numOrders << " matches)...\n";
    auto start = std::chrono::high_resolution_clock::now();

    // 2. Fire Aggressive Buys (Async)
    for (int i = 0; i < numOrders; ++i)
    {
        OrderRequest req;
        req.type = RequestType::Add;
        req.order = Order(numOrders + i, OrderType::GoodTillCancel, 100, 10, Side::Buy);
        queue.push(std::move(req));
    }

    Sync(); // Wait for completion

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Matched " << numOrders << " orders in " << diff.count() << " s\n";
    std::cout << "Throughput: " << (long long)(numOrders / diff.count()) << " matches/sec\n";
}

TEST_F(OrderBookTest, Benchmark_RealWorldScenario)
{
    const int numThreads = 19;
    const int numOps = 50000000;
    const int opsPerThread = numOps / numThreads;

    std::cout << "Starting Real-World Multi-Threaded Benchmark (" << numThreads << " producers, " << numOps
              << " total ops)...\n";
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    producers.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t)
    {
        producers.emplace_back(
            [this, t, opsPerThread]()
            {
                std::mt19937 gen(12345 + t);
                std::uniform_int_distribution<> actionDist(1, 100);
                std::uniform_int_distribution<> qtyDist(1, 100);
                std::uniform_int_distribution<> sideDist(0, 1);

                std::vector<OrderId> localActiveOrderIds;
                localActiveOrderIds.reserve(opsPerThread / 2);

                // Partition Order IDs to avoid collision
                OrderId nextOrderId = (OrderId)t * (OrderId)opsPerThread * 2 + 1;

                for (int i = 0; i < opsPerThread; ++i)
                {
                    int action = actionDist(gen);

                    if (action <= 50)
                    { // 50% Add Passive
                        Side s = sideDist(gen) == 0 ? Side::Buy : Side::Sell;
                        Price p = (s == Side::Buy) ? 99 : 101;

                        OrderRequest req;
                        req.type = RequestType::Add;
                        req.order = Order(nextOrderId, OrderType::GoodTillCancel, p, qtyDist(gen), s);
                        queue.push(std::move(req));

                        localActiveOrderIds.push_back(nextOrderId++);
                    }
                    else if (action <= 70 && !localActiveOrderIds.empty())
                    { // 20% Cancel
                        OrderId idToCancel = localActiveOrderIds.back();
                        localActiveOrderIds.pop_back();

                        OrderRequest req;
                        req.type = RequestType::Cancel;
                        req.order = Order(idToCancel, OrderType::GoodTillCancel, 0, 0, Side::Buy);
                        queue.push(std::move(req));
                    }
                    else
                    { // 30% Aggressive
                        Side s = sideDist(gen) == 0 ? Side::Buy : Side::Sell;
                        Price p = (s == Side::Buy) ? 102 : 98;

                        OrderRequest req;
                        req.type = RequestType::Add;
                        req.order = Order(nextOrderId++, OrderType::GoodTillCancel, p, qtyDist(gen), s);
                        queue.push(std::move(req));
                    }
                }
            });
    }

    for (auto& t : producers)
    {
        if (t.joinable())
            t.join();
    }

    Sync(); // Wait for consumer to drain

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Processed " << (opsPerThread * numThreads) << " ops in " << diff.count() << " s\n";
    std::cout << "Throughput: " << (long long)((opsPerThread * numThreads) / diff.count()) << " ops/sec\n";
}