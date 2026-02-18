#include "Trader/Trader.hpp"

#include <algorithm>

#include "Trader/TraderManager.hpp"

void Trader::setManager(TraderManager* m) { manager_ = m; }
void Trader::stop() { isRunning_ = false; }
bool Trader::isRunning() const { return isRunning_.load(); }

OrderId Trader::placeOrder(OrderType type, Price price, Quantity qty,
                           Side side) {
  OrderId id = manager_->nextOrderId();
  Order order{id, traderId_, type, price, qty, side};
  OrderRequest req{RequestType::Add, order};
  ob_.submitRequest(req);
  orders_.push_back(id);
  if (side == Side::Buy){reservedCash_ += price * qty;}
  else{reservedStock_ += qty;}
  return id;
}

void Trader::cancelOrder(OrderId id) {
  Order order(id, traderId_, OrderType::GoodTillCancel, 0, 0, Side::Buy);
  OrderRequest req{RequestType::Cancel, order};
  ob_.submitRequest(req);

  auto it = std::find(orders_.begin(), orders_.end(), id);
  if (it != orders_.end()) orders_.erase(it);
}

void Trader::modifyOrder(OrderId id, OrderType type, Price price, Quantity qty,
                         Side side) {
  Order order(id, traderId_, type, price, qty, side);
  OrderRequest req{RequestType::Modify, order};
  ob_.submitRequest(req);
}
