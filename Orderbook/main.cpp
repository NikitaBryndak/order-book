#include <functional>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "Constants.hpp"
#include "Order.hpp"
#include "OrderQueue.hpp"
#include "Orderbook.hpp"

// 1. The Queue lives here (Shared between threads)
OrderQueue<OrderRequest> globalQueue;

// 2. The Producer (Simulates a Gateway)
void gatewayThread(int id) {
  for (int i = 0; i < 100; ++i) {
    // Construct Order with correct arguments:
    // OrderId, OrderType, Price, Quantity, Side
    OrderId orderId = static_cast<OrderId>(id * 1000 + i);
    Order order(orderId, OrderType::GoodTillCancel, 100, 10, Side::Buy);

    OrderRequest req{RequestType::Add, std::move(order)};

    // Pushes to the queue (Thread Safe!)
    globalQueue.push(std::move(req));
  }
}

// 3. The Consumer (The Engine)
void engineThread(Orderbook& book) {
  while (true) {
    // Pops from queue (Waits if empty)
    OrderRequest req = globalQueue.pop();

    // Check for termination signal
    if (req.order.getOrderId() == std::numeric_limits<OrderId>::max()) {
      std::cout << "Engine thread received stop signal. Stopping." << std::endl;
      break;
    }

    // Processes into the book (No locks needed here!)
    book.processRequest(req);
  }
}

int main() {
  Orderbook book;

  std::cout << "Starting Orderbook Engine..." << std::endl;

  // Start the Engine (Consumer)
  std::thread consumer(engineThread, std::ref(book));

  // Start Gateways (Producers)
  std::vector<std::thread> producers;
  for (int i = 0; i < 5; i++) {
    producers.emplace_back(gatewayThread, i);
  }

  // Join producers (wait for them to finish sending orders)
  for (auto& p : producers) {
    if (p.joinable()) p.join();
  }

  std::cout << "All producers finished." << std::endl;

  // Send poison pill to stop the consumer
  // Use MAX OrderId to signal stop
  Order stopOrder(std::numeric_limits<OrderId>::max(),
                  OrderType::GoodTillCancel, 0, 0, Side::Buy);
  OrderRequest stopReq{RequestType::Add, std::move(stopOrder)};
  globalQueue.push(std::move(stopReq));

  // Join consumer
  if (consumer.joinable()) consumer.join();

  std::cout << "Final Orderbook Size: " << book.size() << std::endl;

  return 0;
}