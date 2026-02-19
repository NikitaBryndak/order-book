#include "Trader/Trader.hpp"

#include "Trader/TraderManager.hpp"

OrderId Trader::placeOrder(OrderType type, Price price, Quantity qty, Side side) {
  OrderId id = manager_->nextOrderId();
  Order order{id, traderId_, type, price, qty, side};
  OrderRequest req{RequestType::Add, order};
  ob_.submitRequest(req);
  orders_.push_back(id);
  orderIndex_[id] = orders_.size() - 1;
  if (side == Side::Buy) {
    reservedCash_ += price * qty;
  } else {
    reservedStock_ += qty;
  }
  return id;
}

void Trader::cancelOrder(OrderId id) {
  Order order(id, traderId_, OrderType::GoodTillCancel, 0, 0, Side::Buy);
  OrderRequest req{RequestType::Cancel, order};
  ob_.submitRequest(req);

    auto itIdx = orderIndex_.find(id);
  if (itIdx != orderIndex_.end()) {
    size_t idx = itIdx->second;
    size_t last = orders_.size() - 1;
    if (idx != last) {
      // swap with last and update index map (O(1) removal)
      OrderId swapped = orders_[last];
      orders_[idx] = swapped;
      orderIndex_[swapped] = idx;
    }
    orders_.pop_back();
    orderIndex_.erase(itIdx);
  }
}

void Trader::modifyOrder(OrderId id, OrderType type, Price price, Quantity qty,
                         Side side) {
  Order order(id, traderId_, type, price, qty, side);
  OrderRequest req{RequestType::Modify, order};
  ob_.submitRequest(req);
}
