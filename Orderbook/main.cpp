#include "Orderbook/Orderbook.hpp"
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

        Order order(oid, OrderType::GoodTillCancel, price, qty, side);
        OrderRequest req;
        req.type = RequestType::Add;
        req.order = order;

        ob.submitRequest(req);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    const size_t maxOrders = 1000000;
    Orderbook ob(maxOrders, 0, true);

    const int numTraders = 40;
    const int ordersPerTrader = 5;
    std::vector<std::thread> traders;

    for (int i = 0; i < numTraders; ++i) {
        traders.emplace_back(trader, std::ref(ob), i, ordersPerTrader);
    }

    for (auto& t : traders) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}
