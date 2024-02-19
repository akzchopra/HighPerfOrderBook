#ifndef HPORDERBOOK_ORDER_TYPES_H
#define HPORDERBOOK_ORDER_TYPES_H

#pragma once

#include <cstdint>
#include <array>
#include <string_view>
#include <chrono>
#include <arm_neon.h> // for Mac M1

enum class Side : uint8_t {
    BUY,
    SELL
};

enum class OrderType : uint8_t {
    LIMIT,
    MARKET,
    IOC // Immediate or Cancel
};

struct MatchResult;

struct alignas(16) Order {
    static constexpr size_t MAX_ID_LENGTH = 16;
    std::array<char, MAX_ID_LENGTH> id{};
    double price;
    uint32_t quantity;
    Side side;
    OrderType type;
    uint64_t timestamp;

    void set_id(std::string_view id_str) {
        size_t copy_size = std::min(id_str.size(), MAX_ID_LENGTH - 1);
        std::copy_n(id_str.begin(), copy_size, id.begin());
        id[copy_size] = '\0';
    }

    std::string_view get_id() const {
        return std::string_view(id.data());
    }

    // SIMD-optimized comparison using NEON
    bool operator<(const Order& other) const noexcept {
        if (side == Side::BUY) {
            float32x2_t a = vdup_n_f32(static_cast<float>(price));
            float32x2_t b = vdup_n_f32(static_cast<float>(other.price));
            uint32x2_t result = vclt_f32(a, b);
            return vget_lane_u32(result, 0) != 0;
        }
        return price > other.price;
    }

    bool operator>(const Order& other) const noexcept {
        return other < *this;
    }
};

// Price level tracking with NEON SIMD
struct alignas(16) PriceLevel {
    double price;
    uint32_t total_quantity;
    uint32_t order_count;
    uint32_t padding;  // Add padding for 16-byte alignment

    void update_quantity(int32_t delta) noexcept {
        total_quantity += delta;
        order_count += 1;
    }
};

struct MatchResult {
    uint32_t quantity;
    double price;
    std::array<char, Order::MAX_ID_LENGTH> counterparty_id{};

    void set_counterparty_id(std::string_view id) {
        size_t copy_size = std::min(id.size(), Order::MAX_ID_LENGTH - 1);
        std::copy_n(id.begin(), copy_size, counterparty_id.begin());
        counterparty_id[copy_size] = '\0';
    }
};

// SIMD-optimized batch operations
struct BatchOperations {
    static void process_quantity_updates(const std::array<PriceLevel*, 4>& levels,
                                         const std::array<int32_t, 4>& deltas,
                                         size_t count) {
        // Prepare data for NEON processing
        alignas(16) std::array<uint32_t, 4> quantities;
        alignas(16) std::array<uint32_t, 4> counts;

        // Load current values
        for (size_t i = 0; i < count; ++i) {
            if (levels[i]) {
                quantities[i] = levels[i]->total_quantity;
                counts[i] = levels[i]->order_count;
            } else {
                quantities[i] = 0;
                counts[i] = 0;
            }
        }

        // Process using NEON
        uint32x4_t q_vec = vld1q_u32(quantities.data());
        uint32x4_t c_vec = vld1q_u32(counts.data());
        int32x4_t d_vec = vld1q_s32(deltas.data());
        uint32x4_t one_vec = vdupq_n_u32(1);

        // Update quantities and counts
        q_vec = vaddq_u32(q_vec, vreinterpretq_u32_s32(d_vec));
        c_vec = vaddq_u32(c_vec, one_vec);

        // Store results back to arrays
        vst1q_u32(quantities.data(), q_vec);
        vst1q_u32(counts.data(), c_vec);

        // Update price levels
        for (size_t i = 0; i < count; ++i) {
            if (levels[i]) {
                levels[i]->total_quantity = quantities[i];
                levels[i]->order_count = counts[i];
            }
        }
    }

    // Helper method for processing individual updates
    static void process_single_update(PriceLevel* level, int32_t delta) {
        if (!level) return;

        // Load values into NEON registers
        uint32x2_t current = vld1_u32(&level->total_quantity);

        // Create update values
        uint32x2_t update = vcreate_u32((uint64_t)delta | ((uint64_t)1 << 32));

        // Perform addition
        uint32x2_t result = vadd_u32(current, update);

        // Store results back
        vst1_u32(&level->total_quantity, result);
    }
};

static_assert(std::is_trivially_copyable_v<Order>, "Order must be trivially copyable");

#endif //HPORDERBOOK_ORDER_TYPES_H