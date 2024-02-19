#ifndef HPORDERBOOK_ORDER_BOOK_H
#define HPORDERBOOK_ORDER_BOOK_H

#pragma once

#include <map>
#include <shared_mutex>
#include <vector>
#include <optional>
#include <atomic>

#include "order_types.h"
#include "lock_free_queue.h"

template<typename PriceType>
class OrderBook {
public:
    static constexpr size_t MAX_ORDERS = 1'000'000;
    static constexpr size_t SIMD_WIDTH = 4; // Processes 4 elements at a time

private:
    // Lock-free queue for incoming orders
    LockFreeQueue<Order, MAX_ORDERS> incoming_orders_;

    // Price level tracking
    std::map<PriceType, PriceLevel> bids_;
    std::map<PriceType, PriceLevel> asks_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Order tracking
    std::atomic<uint32_t> next_order_id_{0};

    // SIMD-optimized batch processing of limit orders
    void process_limit_orders_batch(const std::vector<Order>& orders) {
        std::unique_lock lock(mutex_);

        alignas(16) std::array<int32_t, SIMD_WIDTH> deltas{};
        alignas(16) std::array<PriceLevel*, SIMD_WIDTH> levels{};

        size_t batch_size = 0;
        for (const auto& order : orders) {
            auto& book = (order.side == Side::BUY) ? bids_ : asks_;
            auto [it, inserted] = book.try_emplace(order.price,
                                                   PriceLevel{order.price, 0, 0, 0}); // Initialize with padding

            levels[batch_size] = &(it->second);
            deltas[batch_size] = order.quantity;

            batch_size++;
            if (batch_size == SIMD_WIDTH) {
                BatchOperations::process_quantity_updates(levels, deltas, SIMD_WIDTH);
                batch_size = 0;
            }
        }

        if (batch_size > 0) {
            BatchOperations::process_quantity_updates(levels, deltas, batch_size);
        }
    }

    // SIMD-optimized price matching
    std::vector<MatchResult> match_market_order_simd(const Order& order) {
        std::unique_lock lock(mutex_);
        std::vector<MatchResult> matches;

        auto& book = (order.side == Side::BUY) ? asks_ : bids_;
        uint32_t remaining = order.quantity;

        // Process matches
        for (auto it = book.begin(); it != book.end() && remaining > 0;) {
            auto& level = it->second;
            uint32_t matched = std::min(remaining, level.total_quantity);

            if (matched > 0) {
                MatchResult match;
                match.quantity = matched;
                match.price = level.price;
                match.set_counterparty_id(order.get_id());
                matches.push_back(match);

                level.update_quantity(-static_cast<int32_t>(matched));
                remaining -= matched;
            }

            if (level.total_quantity == 0) {
                it = book.erase(it);
            } else {
                ++it;
            }
        }

        return matches;
    }

    // Helper functions to get best prices considering direction
    PriceType get_best_bid() const {
        if (bids_.empty()) return 0;
        // For bids, we want the highest price
        return std::max_element(bids_.begin(), bids_.end(),
                                [](const auto& a, const auto& b) { return a.first < b.first; })->first;
    }

    PriceType get_best_ask() const {
        if (asks_.empty()) return 0;
        // For asks, we want the lowest price
        return std::min_element(asks_.begin(), asks_.end(),
                                [](const auto& a, const auto& b) { return a.first < b.first; })->first;
    }

public:
    OrderBook() = default;

    // Add a limit order
    bool add_limit_order(Side side, PriceType price, uint32_t quantity,
                         std::string_view id) {
        Order order;
        order.set_id(id);
        order.price = price;
        order.quantity = quantity;
        order.side = side;
        order.type = OrderType::LIMIT;
        order.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        std::vector<Order> batch_orders{order};
        process_limit_orders_batch(batch_orders);
        return true;
    }

    // Process a market order
    std::vector<MatchResult> process_market_order(Side side, uint32_t quantity,
                                                  std::string_view id) {
        Order order;
        order.set_id(id);
        order.price = 0.0;
        order.quantity = quantity;
        order.side = side;
        order.type = OrderType::MARKET;
        order.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        return match_market_order_simd(order);
    }

    // Get current best bid/ask prices
    std::pair<PriceType, PriceType> get_best_prices() const {
        std::shared_lock lock(mutex_);
        return {get_best_bid(), get_best_ask()};
    }

    // Get current depth at price level
    std::vector<PriceLevel> get_depth(Side side, size_t levels = 5) const {
        std::shared_lock lock(mutex_);
        std::vector<PriceLevel> depth;
        const auto& book = (side == Side::BUY) ? bids_ : asks_;

        std::vector<std::pair<PriceType, PriceLevel>> sorted_levels(book.begin(), book.end());
        if (side == Side::BUY) {
            // Sort bids in descending order
            std::sort(sorted_levels.begin(), sorted_levels.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
        } else {
            // Sort asks in ascending order
            std::sort(sorted_levels.begin(), sorted_levels.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
        }

        for (size_t i = 0; i < levels && i < sorted_levels.size(); ++i) {
            depth.push_back(sorted_levels[i].second);
        }

        return depth;
    }
};

#endif //HPORDERBOOK_ORDER_BOOK_H
