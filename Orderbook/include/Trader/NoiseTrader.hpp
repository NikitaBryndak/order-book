#include <random>
#include "Trader.hpp"
#include "Config.hpp"

class NoiseTrader : public Trader
{
public:
    NoiseTrader(uint32_t s) : Trader(s, Strategy::Noise), nextOrderId_(1), rng_(std::random_device{}()) {};
    void run(Orderbook& ob);

private:
    OrderId nextOrderId_;
    std::mt19937 rng_;
};
