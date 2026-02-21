#include "Orderbook/Orderbook.hpp"

#include <cstdio>
#include <stdexcept>
#include <utility>

#include "utils.hpp"


/// @brief Orderbook matching function that uses map bid and ask levels to match orders
/// @param newOrder `OrderPointer` that is inserted into the matching engine
void Orderbook::matchOrders(OrderPointer newOrder) {
  if (newOrder->getSide() == Side::Buy) {
    while (true) {
      if (asks_.empty() || newOrder->isFilled()) {
        break;
      }

      auto& [bestAskPrice, asks] = *asks_.begin();

      // Lazy cleanup of cancelled orders
      if (!asks.front()->isValid()) [[unlikely]] {
        auto cancelled = asks.front();
        asks.pop_front();

        if (asks.empty()) {
          askLevels_.erase(bestAskPrice);
          asks_.erase(asks_.begin());
        }
        orderPool_.release(cancelled);
        continue;
      }

      if (newOrder->getPrice() < bestAskPrice) {
        break;
      }

      Quantity fillQuantity =
          std::min(newOrder->getQuantity(), asks.front()->getQuantity());

      onMatch(newOrder, asks.front(), fillQuantity);
      newOrder->Fill(fillQuantity);
      asks.front()->Fill(fillQuantity);
      askLevels_[bestAskPrice] -= fillQuantity;

      if (asks.front()->isFilled()) {
        OrderPointer filled = asks.front();
        orders_.erase(filled->getOrderId());
        asks.pop_front();
        size_--;
        orderPool_.release(filled);
      }

      if (asks.empty()) {
        askLevels_.erase(bestAskPrice);
        asks_.erase(asks_.begin());
      }
    }
  } else {
    while (true) {
      if (bids_.empty() || newOrder->isFilled()) {
        break;
      }

      auto& [bestBidPrice, bids] = *bids_.begin();

      // Lazy cleanup of cancelled orders
      if (!bids.front()->isValid()) {
        auto cancelled = bids.front();
        bids.pop_front();
        if (bids.empty()) {
          bidLevels_.erase(bestBidPrice);
          bids_.erase(bids_.begin());
        }
        orderPool_.release(cancelled);
        continue;
      }

      if (newOrder->getPrice() > bestBidPrice) {
        break;
      }

      Quantity fillQuantity =
          std::min(newOrder->getQuantity(), bids.front()->getQuantity());

      onMatch(bids.front(), newOrder, fillQuantity);

      newOrder->Fill(fillQuantity);
      bids.front()->Fill(fillQuantity);
      bidLevels_[bestBidPrice] -= fillQuantity;

      if (bids.front()->isFilled()) {
        OrderPointer filled = bids.front();
        orders_.erase(filled->getOrderId());
        bids.pop_front();
        size_--;
        orderPool_.release(filled);
      }

      if (bids.empty()) {
        bidLevels_.erase(bestBidPrice);
        bids_.erase(bids_.begin());
      }
    }
  }
};

void Orderbook::addOrder(const Order& order) {
  OrderPointer orderPtr = orderPool_.acquire(
      order.getOrderId(), order.getOwner(), order.getOrderType(),
      order.getPrice(), order.getQuantity(), order.getSide());

  if (!orderPtr) {
    throw std::runtime_error("Out of orders");
  }

  matchOrders(orderPtr);

  if (!orderPtr->isFilled() &&
      order.getOrderType() == OrderType::GoodTillCancel) {
    if (order.getSide() == Side::Buy) {
      bids_[order.getPrice()].push_back(orderPtr);
      bidLevels_[order.getPrice()] += order.getQuantity();
    } else {
      asks_[order.getPrice()].push_back(orderPtr);
      askLevels_[order.getPrice()] += order.getQuantity();
    }

    orders_[order.getOrderId()] = orderPtr;
    size_++;
  } else {
    orderPool_.release(orderPtr);
  }
}

void Orderbook::cancelOrder(const OrderId& orderId) {
  if (orders_.count(orderId)) {
    OrderPointer order = orders_[orderId];
    Price price = order->getPrice();
    Quantity qty = order->getQuantity();

    if (order->getSide() == Side::Buy) {
      bidLevels_[price] -= qty;
      if (bidLevels_[price] == 0) bidLevels_.erase(price);

      if (bids_.count(price)) {
        auto& list = bids_[price];
        for (auto it = list.begin(); it != list.end(); ++it) {
          if (*it == order) {
            list.erase(it);
            break;
          }
        }
        if (list.empty()) bids_.erase(price);
      }
    } else {
      askLevels_[price] -= qty;
      if (askLevels_[price] == 0) askLevels_.erase(price);

      if (asks_.count(price)) {
        auto& list = asks_[price];
        for (auto it = list.begin(); it != list.end(); ++it) {
          if (*it == order) {
            list.erase(it);
            break;
          }
        }
        if (list.empty()) asks_.erase(price);
      }
    }

    order->cancel();

    orders_.erase(orderId);
    size_--;
    orderPool_.release(order);
  }
}

void Orderbook::modifyOrder(const Order& order) {
  this->cancelOrder(order.getOrderId());
  this->addOrder(order);
}

void Orderbook::submitRequest(OrderRequest& request) {
  buffer_.push(std::move(request));
}

void Orderbook::processLoop() {
  while (true) {
    OrderRequest request = buffer_.pop();
    switch (request.type) {
      case (RequestType::Add):
        this->addOrder(request.order);
        break;

      case (RequestType::Cancel):
        this->cancelOrder(request.order.getOrderId());
        break;

      case (RequestType::Modify):
        this->modifyOrder(request.order);
        break;

#ifdef OB_ENABLE_UI
      case (RequestType::Snapshot):
        this->takeSnapshot();
        break;
#endif

      case (RequestType::Stop):
        return;
    }
  }
}

Price Orderbook::topBidPrice() const {
  if (bids_.empty()) return 0;
  return bids_.begin()->first;
}

Price Orderbook::topAskPrice() const {
  if (asks_.empty()) return 0;
  return asks_.begin()->first;
}

inline void Orderbook::onMatch(const OrderPointer& b, const OrderPointer& a, Quantity& qty) {
  matchedTrades_++;

#ifdef OB_ENABLE_UI
  recordTradePrice(a->getPrice(), qty);
#endif

  if (listener_) [[likely]] {
    Trade t{b, a, qty};
    listener_(t);
  }
}

Orderbook::Orderbook(size_t maxOrders, int coreId)
    : orderPool_(maxOrders),
      buffer_(nextPowerOf2(maxOrders)) {
  workerThread_ = std::thread(&Orderbook::processLoop, this);

  if (coreId >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);

    int rc = pthread_setaffinity_np(workerThread_.native_handle(),
                                    sizeof(cpu_set_t), &cpuset);

    if (rc != 0) {
      std::fprintf(stderr, "Error calling pthread_setaffinity_np: %d\n", rc);
    }
  }
}

Orderbook::~Orderbook() {
  OrderRequest stop;
  stop.type = RequestType::Stop;

  submitRequest(stop);

  if (workerThread_.joinable()) {
    workerThread_.join();
  }
}
