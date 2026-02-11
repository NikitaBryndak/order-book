#include "Orderbook/Orderbook.hpp"
#include "Trader/Trader.hpp"
#include "Trader/NoiseTrader.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include <chrono>

void trader(Orderbook& ob, int id, int numOrders) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<Price> priceDist(350, 400);
    std::uniform_int_distribution<Quantity> qtyDist(10, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    for (int i = 0; i < numOrders; ++i) {
        Side side = (sideDist(rng) == 0) ? Side::Buy : Side::Sell;
        Price price = priceDist(rng);
        Quantity qty = qtyDist(rng);
        OrderId oid = (static_cast<OrderId>(id) << 32) | i;


        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {

    int ntq = 40;
    int wtq = 5;
    int mtq = 20;

    Orderbook ob(1 << 19, 0, false);

    std::vector<Trader> traders;

    // ntq
    for (int i = 0; i < ntq; ++i) {
        traders.emplace_back(new NoiseTrader(10000));
    }

    while (true)
    {

    }



    for (auto& t : traders) {
        t.join();
    }


    return 0;
}
