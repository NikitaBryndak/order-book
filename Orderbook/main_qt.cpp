// main_qt.cpp  –  Qt6 entry point for the order-book simulation
#include "Orderbook/OrderbookUI.hpp"
#include "Trader/TraderManager.hpp"
#include "Trader/NoiseTrader.hpp"

#include <QApplication>
#include <cstdlib>
#include <limits>
#include <memory>

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  const int nTraders = (argc > 1) ? std::atoi(argv[1]) : 10;

  Orderbook ob(1 << 20, 0);
  TraderManager mgr(ob, 1000);

  const uint64_t infiniteCash = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < nTraders; ++i)
    mgr.addTrader(std::make_shared<NoiseTrader>(i + 1, infiniteCash, ob));

  mgr.start();

  OrderBookWindow win(ob);
  win.show();

  int ret = app.exec();

  mgr.stop();
  mgr.join();

  return ret;
}
