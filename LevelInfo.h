
#pragma once

#include "Constants.h"
#include <vector>

struct LevelInfo {
    Price _price;
    Quantity _quantity;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos {
    public:
        OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) : bids_(bids), asks_(asks) {};
        const LevelInfos& GetBids() const { return bids_; };
        const LevelInfos& GetAsks() const { return asks_; };

    private:
        LevelInfos bids_;
        LevelInfos asks_;
};