#include "Orderbook.hpp"
#include "Order.hpp"
#include "Constants.hpp"
#include "utils.hpp"
#include <stdexcept>

void Orderbook::matchOrders(OrderPointer newOrder)
{
    if (newOrder->getSide() == Side::Buy)
    {
        while (true)
        {
            if (asks_.empty() || newOrder->isFilled())
            {
                break;
            }

            auto &[bestAskPrice, asks] = *asks_.begin();

            // Lazy cleanup of cancelled orders
            if (!asks.front()->isValid())
            {
                auto cancelled = asks.front();
                asks.pop_front();
                if (asks.empty())
                {
                    asks_.erase(asks_.begin());
                }
                orderPool_.release(cancelled);
                continue;
            }

            if (newOrder->getPrice() < bestAskPrice)
            {
                break;
            }

            Quantity fillQuantity = std::min(newOrder->getQuantity(), asks.front()->getQuantity());

            newOrder->Fill(fillQuantity);
            asks.front()->Fill(fillQuantity);

            if (asks.front()->isFilled())
            {
                OrderPointer filled = asks.front();
                orders_.erase(filled->getOrderId());
                asks.pop_front();
                size_--;
                orderPool_.release(filled);
            }

            if (asks.empty())
            {
                asks_.erase(asks_.begin());
            }
        }
    }
    else
    {
        while (true)
        {
            if (bids_.empty() || newOrder->isFilled())
            {
                break;
            }

            auto &[bestBidPrice, bids] = *bids_.begin();

            // Lazy cleanup of cancelled orders
            if (!bids.front()->isValid())
            {
                auto cancelled = bids.front();
                bids.pop_front();
                if (bids.empty())
                {
                    bids_.erase(bids_.begin());
                }
                orderPool_.release(cancelled);
                continue;
            }

            if (newOrder->getPrice() > bestBidPrice)
            {
                break;
            }

            Quantity fillQuantity = std::min(newOrder->getQuantity(), bids.front()->getQuantity());

            newOrder->Fill(fillQuantity);
            bids.front()->Fill(fillQuantity);

            if (bids.front()->isFilled())
            {
                OrderPointer filled = bids.front();
                orders_.erase(bids.front()->getOrderId());
                bids.pop_front();
                size_--;
                orderPool_.release(filled);
            }

            if (bids.empty())
            {
                bids_.erase(bids_.begin());
            }
        }
    }
};

void Orderbook::addOrder(const Order &order)
{
    OrderPointer orderPtr = orderPool_.acquire(
        order.getOrderId(),
        order.getOrderType(),
        order.getPrice(),
        order.getQuantity(),
        order.getSide()
    );

    if (!orderPtr)
    {
        throw std::runtime_error("Out of orders");
    }

    matchOrders(orderPtr);

    if (!orderPtr->isFilled() && order.getOrderType() == OrderType::GoodTillCancel)
    {
        if (order.getSide() == Side::Buy)
        {
            bids_[order.getPrice()].push_back(orderPtr);
        }
        else
        {
            asks_[order.getPrice()].push_back(orderPtr);
        }

        orders_[order.getOrderId()] = orderPtr;
        size_++;
    }
    else
    {
        orderPool_.release(orderPtr);
    }
}

void Orderbook::cancelOrder(const OrderId &orderId)
{
    if (orders_.count(orderId))
    {
        orders_[orderId]->cancel();
        orders_.erase(orderId);
        size_--;
    }
}

void Orderbook::modifyOrder(const Order &order)
{
    this->cancelOrder(order.getOrderId());
    this->addOrder(order);
}

void Orderbook::submitRequest(OrderRequest &request)
{
    buffer_.push(std::move(request));
}

void Orderbook::processLoop()
{
    while (true)
    {
        OrderRequest request = buffer_.pop();
        switch (request.type)
        {
        case (RequestType::Add):
            this->addOrder(request.order);
            break;

        case (RequestType::Cancel):
            this->cancelOrder(request.order.getOrderId());
            break;

        case (RequestType::Modify):
            this->modifyOrder(request.order);
            break;
        case (RequestType::Stop):
            return;
        }
    }
}

Orderbook::Orderbook(size_t maxOrders) : orderPool_(maxOrders), buffer_(nextPowerOf2(maxOrders)){
    workerThread_ = std::thread(&Orderbook::processLoop, this);
}

Orderbook::~Orderbook()
{
    OrderRequest stop;
    stop.type = RequestType::Stop;

    submitRequest(stop);

    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}