#pragma once

#include <vector>
#include "Constants.h"
#include "TradeInfo.h"

class Trade {
    public:
        Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade):
            askTrade_ {askTrade},
            bidTrade_ {bidTrade}
        {

        };

        const TradeInfo& GetBidTrade() const { return bidTrade_; }
        const TradeInfo& GetAskTrade() const { return askTrade_; }
        friend std::ostream& operator<<(std::ostream& os, const Trade& trade);

    private:
        TradeInfo askTrade_;
        TradeInfo bidTrade_;
};

using Trades = std::vector<Trade>;