#include <iostream>
#include <thread>
#include <vector>
#include <chrono> // <--- NEW: For timing

#include "Orderbook.hpp"
#include "Order.hpp"

int main()
{
    Orderbook orderbook;
    const int NUM_THREADS = 200;
    const int ORDERS_PER_THREAD = 10000; // Increased load

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    std::cout << "Starting benchmark with " << NUM_THREADS << " threads processing "
              << (NUM_THREADS * ORDERS_PER_THREAD) << " orders..." << std::endl;

    // 1. Start the Timer
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back([&orderbook, i, ORDERS_PER_THREAD]() {
            for (int j = 0; j < ORDERS_PER_THREAD; ++j)
            {
                OrderId id = (i * ORDERS_PER_THREAD) + j;
                // Alternate Buy/Sell to trigger matching logic too
                Side side = (j % 2 == 0) ? Side::Buy : Side::Sell;
                Order order(id, 100, 10, side);
                orderbook.addOrder(order);
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // 2. Stop the Timer
    auto endTime = std::chrono::high_resolution_clock::now();

    // 3. Calculate Duration
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Total Time: " << duration.count() << "ms" << std::endl;
    std::cout << "Final Size: " << orderbook.size() << std::endl;

    // 4. Calculate Orders Per Second
    if (duration.count() > 0) {
        unsigned long long ops = (NUM_THREADS * ORDERS_PER_THREAD * 1000) / duration.count();
        std::cout << "Throughput: " << ops << " orders/second" << std::endl;
    }

    return 0;
}