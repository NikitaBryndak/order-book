#pragma once
#include <vector>

template <typename T> class OrderPool
{
private:
    struct Block
    {
        T object;
        Block* next;
    };

    std::vector<Block> pool_;
    Block* freehead_;

public:
    OrderPool(size_t size) {
        pool_.resize(size);
        freehead_ = &pool_[0];

        for (size_t i = 0; i < size - 1; i++)
        {
            pool_[i].next = &pool_[i+1];
        }
        pool_[size - 1].next = nullptr;
    }

    template <typename... Args>
    T* acquire(Args&&... args)
    {
        if (freehead_ == nullptr) { return nullptr; };

        Block* block = freehead_;
        freehead_ = block->next;

        T* ptr = &(block->object);
        // Pass R-Values to the Order constructor and write it in the place of the ptr
        new (ptr) T(std::forward<Args>(args)...);

        return ptr;
    }

    void release(T* ptr)
    {
        ptr->~T();

        Block* block = (Block*)ptr;

        block->next = freehead_;
        freehead_ = block;
    }

};