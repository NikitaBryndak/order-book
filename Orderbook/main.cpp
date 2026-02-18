#include "Orderbook/Orderbook.hpp"
#include "Orderbook/Order.hpp"
#include "Trader/TraderManager.hpp"
#include "Trader/NoiseTrader.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <limits>
#include <cstdlib>

int main(int argc, char** argv) {
    const int nTraders = (argc > 1) ? std::atoi(argv[1]) : 10;
    const int runSecs = (argc > 2) ? std::atoi(argv[2]) : 5;

    // create orderbook and trader manager
    Orderbook ob(1 << 20, -1, true);
    TraderManager mgr(ob, /*sleepUs=*/1000);

    // NoiseTraders with "infinite" money
    const uint64_t infiniteCash = std::numeric_limits<uint64_t>::max();

    for (int i = 0; i < nTraders; ++i) {
        mgr.addTrader(std::make_unique<NoiseTrader>(static_cast<uint32_t>(i + 1), infiniteCash, ob));
    }

    mgr.start();
    std::cout << "Started " << nTraders << " NoiseTraders (infinite cash) for " << runSecs << "s...\n";

    // Send a Print request to the Orderbook every second while traders run
    auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(runSecs);
    while (std::chrono::steady_clock::now() < endTime) {
        OrderRequest printReq{RequestType::Print, Order()};
        ob.submitRequest(printReq);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // final state print
    OrderRequest finalPrint{RequestType::Print, Order()};
    ob.submitRequest(finalPrint);

    mgr.stop();
    mgr.join();

    std::cout << "Done. Resting orders: " << ob.size()
              << "  TotalMatches=" << ob.matchedTrades()
              << "  TopBid=" << ob.topBidPrice()
              << "  TopAsk=" << ob.topAskPrice() << "\n";
    return 0;
}
