#include "Trader/NoiseTrader.hpp"

void NoiseTrader::run(Orderbook& ob)
{
    if (curProtfolioSize_ > 0)
    {
        std::uniform_int_distribution<int> sideDist(0, 1);
        std::uniform_int_distribution<Quantity> qtySpread(1, Config::nQSpread);
        std::uniform_int_distribution<Price> priceSpread(-Config::nPSpread, Config::nPSpread);

        Side side = (sideDist(rng_) == 0) ? Side::Sell : Side::Buy;
        Quantity qty = qtySpread(rng_);
        Price spread = priceSpread(rng_);

        Price midPrice = (ob.topAskPrice() + ob.topBidPrice()) / 2;
        Price price = midPrice + spread;

        // Ensure price is positive
        if (price <= 0) return;

        // Check if we have sufficient portfolio before placing order
        uint64_t cost = qty * price;
        if (side == Side::Buy && curProtfolioSize_ < cost) return;
        if (side == Side::Sell && positionSize_ < qty) return;

        OrderId oid = nextOrderId_++;
        Order order(oid, OrderType::GoodTillCancel, price, qty, side);

        OrderRequest req;
        req.type = RequestType::Add;
        req.order = order;

        ob.submitRequest(req);
        orders_.insert(oid);
    }
};