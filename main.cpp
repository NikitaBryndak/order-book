#include "Orderbook.hpp"
#include "Order.hpp"
#include "Constants.hpp"
#include <iostream>
#include <algorithm> // Needed for std::min (though likely included in Orderbook.cpp)

int main() {
    Orderbook orderbook;

    // --- TEST 1: No Cross & Initial Size Check ---
    std::cout << "--- TEST 1: No Cross ---\n";
    Order t1_buy(1, 100, 10, Side::Buy);
    Order t1_sell(2, 101, 5, Side::Sell); // Sell is higher than Buy (no cross)

    orderbook.addOrder(t1_buy);
    orderbook.addOrder(t1_sell);

    // Expected: 2 orders (1 Bid, 1 Ask)
    std::cout << "Size after no cross: " << orderbook.size() << " (Expected: 2)\n";

    // --- TEST 2: Full Fill Match ---
    std::cout << "\n--- TEST 2: Full Fill Match ---\n";
    Order t2_aggressor(3, 100, 15, Side::Sell); // Aggressively crosses Best Bid (100)

    orderbook.addOrder(t2_aggressor);

    // Expected: t1_buy (10 Qty) is completely filled and removed. t2_aggressor (15 Qty) fills 10 and places 5 remaining at 100. t1_sell @ 101 remains.
    // Book state: 1 Buy @ 101 (t1_sell), 1 Sell @ 100 (t2_aggressor remaining 5).
    std::cout << "Size after execution: " << orderbook.size() << " (Expected: 2)\n";

    // --- TEST 3: Partial Fill ---
    std::cout << "\n--- TEST 3: Partial Fill ---\n";
    Order t3_partial(4, 101, 2, Side::Buy); // Partially fills t1_sell (now at 3 Qty)

    orderbook.addOrder(t3_partial);

    // Expected: t3_partial removes 2 Qty from t1_sell (which had 5 Qty).
    // t1_sell (Ask @ 101) is partially filled and remaining is 3.
    // Book state: 1 Sell @ 100 (t2_aggressor), 1 Sell @ 101 (t1_sell remaining 3 Qty). NO MATCH.
    std::cout << "Size after partial match: " << orderbook.size() << " (Expected: 2)\n";

    // --- TEST 4: O(1) Cancellation Check ---
    std::cout << "\n--- TEST 4: Cancellation Check ---\n";

    // t1_sell (OrderID 2) is the Sell order placed in Test 1 (with 3 Qty left).
    orderbook.cancelOrder(2);

    // Expected: Order 2 is removed from the lookup map and marked as invalid. size_ drops by 1.
    std::cout << "Size after cancellation (ID 2): " << orderbook.size() << " (Expected: 1)\n";

    return 0;
}