#include <cstdint>
#include "Constants.hpp"
#include <set>
#include "Orderbook/Orderbook.hpp"
class Trader
{
public:
    Trader(uint32_t protfolioSize, Strategy st) : protfolioSize_(protfolioSize), curProtfolioSize_(protfolioSize), strategy_(st) {}

    /* ----------------------------- Getters & Setters ---------------------------- */
    Strategy getStrategy() const { return strategy_; };
    uint32_t getInitialPortfolioSize() const { return protfolioSize_; };
    uint32_t getProtfolioSize() const { return curProtfolioSize_; };

    virtual void run(Orderbook& ob);  // Pure virtual for strategy-specific behavior

protected:
    uint32_t protfolioSize_;
    uint32_t curProtfolioSize_;
    uint32_t positionSize_;
    Strategy strategy_;
    std::set<OrderId> orders_;

};