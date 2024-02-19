#ifndef HPORDERBOOK_LOCK_FREE_QUEUE_H
#define HPORDERBOOK_LOCK_FREE_QUEUE_H

#pragma once

#include <atomic>
#include <array>
#include <optional>
#include <iostream>
#include "order_types.h"

template<typename T, size_t N>
class LockFreeQueue {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

private:
    struct alignas(64) Node {
        T data;
        std::atomic<uint64_t> sequence;
    };

    static constexpr uint64_t EMPTY_SEQUENCE = ~0ULL;
    static constexpr size_t BUFFER_MASK = N - 1;

    alignas(64) std::atomic<uint64_t> head_{0};
    alignas(64) std::atomic<uint64_t> tail_{0};
    std::vector<Node> buffer_;

public:
    LockFreeQueue() : buffer_(N) {
        try {
            for (auto& node : buffer_) {
                node.sequence.store(EMPTY_SEQUENCE, std::memory_order_relaxed);
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize queue: " << e.what() << std::endl;
            throw;
        }
    }

    bool try_enqueue(const T& data) noexcept {
        uint64_t tail = tail_.load(std::memory_order_relaxed);
        Node& node = buffer_[tail & BUFFER_MASK];
        uint64_t seq = node.sequence.load(std::memory_order_acquire);

        if (seq == tail) {
            if (tail_.compare_exchange_strong(tail, tail + 1,
                                              std::memory_order_relaxed)) {
                node.data = data;
                node.sequence.store(tail + 1, std::memory_order_release);
                return true;
            }
        }
        return false;
    }

    std::optional<T> try_dequeue() noexcept {
        uint64_t head = head_.load(std::memory_order_relaxed);
        Node& node = buffer_[head & BUFFER_MASK];
        uint64_t seq = node.sequence.load(std::memory_order_acquire);

        if (seq == head + 1) {
            if (head_.compare_exchange_strong(head, head + 1,
                                              std::memory_order_relaxed)) {
                T result = node.data;
                node.sequence.store(head + N, std::memory_order_release);
                return result;
            }
        }
        return std::nullopt;
    }
};

#endif //HPORDERBOOK_LOCK_FREE_QUEUE_H
