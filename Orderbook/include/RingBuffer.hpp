#pragma once
#include <vector>
#include <atomic>
#include <thread>
#include <utility>
#include <stdexcept>
#include <memory>

template <typename T>
class RingBuffer
{
private:
    struct alignas(64) Slot
    {
        std::atomic<bool> written{false};
        T data;
    };

    std::unique_ptr<Slot[]> buffer_;

    // BITMASK
    const size_t capacity_;
    const size_t mask_;

    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) size_t tail_{0};

public:

    RingBuffer(size_t capacity) : capacity_(capacity), mask_(capacity-1)
    {
        if ((capacity & (capacity - 1)) != 0)
        {
            throw std::runtime_error("RingBuffer capacity must be power of 2");
        }
        buffer_ = std::make_unique<Slot[]>(capacity);
    }

    void push(T&& item)
    {
        size_t headIdx = head_.fetch_add(1, std::memory_order_relaxed);
        size_t index = headIdx & mask_;

        while (buffer_[index].written.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        buffer_[index].data = std::move(item);

        buffer_[index].written.store(true, std::memory_order_release);
    }

    T pop() {
        size_t index = tail_ & mask_;

        while (!buffer_[index].written.load(std::memory_order_acquire))
        {
            std::atomic_thread_fence(std::memory_order_acquire);
        }

        T item = std::move(buffer_[index].data);

        buffer_[index].written.store(false, std::memory_order_release);

        tail_++;

        return item;
    }
};
