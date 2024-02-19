#include <gtest/gtest.h>
#include <thread>
#include <future>

#include "../include/order_book.h"

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook<double> book;
};

// Basic Order Book Operations
TEST_F(OrderBookTest, BasicLimitOrder) {
ASSERT_TRUE(book.add_limit_order(Side::BUY, 100.0, 1000, "ORDER1"));

auto [bid, ask] = book.get_best_prices();
EXPECT_EQ(bid, 100.0);
EXPECT_EQ(ask, 0.0);

ASSERT_TRUE(book.add_limit_order(Side::SELL, 101.0, 1000, "ORDER2"));
std::tie(bid, ask) = book.get_best_prices();
EXPECT_EQ(bid, 100.0);
EXPECT_EQ(ask, 101.0);
}

// Price Level Management
TEST_F(OrderBookTest, PriceLevels) {
ASSERT_TRUE(book.add_limit_order(Side::BUY, 100.0, 1000, "ORDER1"));
ASSERT_TRUE(book.add_limit_order(Side::BUY, 100.0, 500, "ORDER2"));

auto depth = book.get_depth(Side::BUY, 1);
ASSERT_EQ(depth.size(), 1);
EXPECT_EQ(depth[0].total_quantity, 1500);
EXPECT_EQ(depth[0].order_count, 2);
}

// Market Order Matching
TEST_F(OrderBookTest, MarketOrderMatching) {
// Add some limit orders
ASSERT_TRUE(book.add_limit_order(Side::SELL, 100.0, 500, "ORDER1"));
ASSERT_TRUE(book.add_limit_order(Side::SELL, 101.0, 500, "ORDER2"));
ASSERT_TRUE(book.add_limit_order(Side::SELL, 102.0, 500, "ORDER3"));

// Process market buy order
auto matches = book.process_market_order(Side::BUY, 800, "MARKET1");

ASSERT_EQ(matches.size(), 2);
EXPECT_EQ(matches[0].quantity, 500);
EXPECT_EQ(matches[0].price, 100.0);
EXPECT_EQ(matches[1].quantity, 300);
EXPECT_EQ(matches[1].price, 101.0);
}

// Depth Management
TEST_F(OrderBookTest, DepthManagement) {
// Add orders at different price levels
ASSERT_TRUE(book.add_limit_order(Side::BUY, 100.0, 1000, "ORDER1"));
ASSERT_TRUE(book.add_limit_order(Side::BUY, 99.0, 1000, "ORDER2"));
ASSERT_TRUE(book.add_limit_order(Side::BUY, 98.0, 1000, "ORDER3"));

auto depth = book.get_depth(Side::BUY, 3);
ASSERT_EQ(depth.size(), 3);
EXPECT_EQ(depth[0].price, 100.0);
EXPECT_EQ(depth[1].price, 99.0);
EXPECT_EQ(depth[2].price, 98.0);
}

// Concurrent Order Processing
TEST_F(OrderBookTest, ConcurrentOrders) {
constexpr size_t NUM_ORDERS = 1000;
constexpr size_t NUM_THREADS = 4;
std::atomic<size_t> success_count{0};

auto submit_orders = [&](Side side) {
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        double price = 100.0 + (i % 10);
        if (book.add_limit_order(side, price, 100, "ORDER" + std::to_string(i))) {
            success_count++;
        }
    }
};

std::vector<std::future<void>> futures;
for (size_t i = 0; i < NUM_THREADS; ++i) {
Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
futures.push_back(std::async(std::launch::async, submit_orders, side));
}

for (auto& future : futures) {
future.wait();
}

EXPECT_EQ(success_count, NUM_ORDERS * NUM_THREADS);
}

// Test Market Order with Insufficient Liquidity
TEST_F(OrderBookTest, InsufficientLiquidity) {
ASSERT_TRUE(book.add_limit_order(Side::SELL, 100.0, 500, "ORDER1"));

auto matches = book.process_market_order(Side::BUY, 1000, "MARKET1");
ASSERT_EQ(matches.size(), 1);
EXPECT_EQ(matches[0].quantity, 500);
}

// Test Price Level Updates After Matches
TEST_F(OrderBookTest, PriceLevelUpdatesAfterMatch) {
ASSERT_TRUE(book.add_limit_order(Side::SELL, 100.0, 1000, "ORDER1"));

auto matches = book.process_market_order(Side::BUY, 600, "MARKET1");

auto depth = book.get_depth(Side::SELL, 1);
ASSERT_EQ(depth.size(), 1);
EXPECT_EQ(depth[0].total_quantity, 400);
}

// Test Order Book State After Multiple Operations
TEST_F(OrderBookTest, OrderBookState) {
// Add various orders
ASSERT_TRUE(book.add_limit_order(Side::BUY, 99.0, 1000, "ORDER1"));
ASSERT_TRUE(book.add_limit_order(Side::BUY, 98.0, 1000, "ORDER2"));
ASSERT_TRUE(book.add_limit_order(Side::SELL, 101.0, 1000, "ORDER3"));
ASSERT_TRUE(book.add_limit_order(Side::SELL, 102.0, 1000, "ORDER4"));

// Process some market orders
book.process_market_order(Side::BUY, 500, "MARKET1");
book.process_market_order(Side::SELL, 500, "MARKET2");

// Verify final state
auto [bid, ask] = book.get_best_prices();
auto bid_depth = book.get_depth(Side::BUY, 2);
auto ask_depth = book.get_depth(Side::SELL, 2);

EXPECT_GT(bid, 0);
EXPECT_GT(ask, bid);
ASSERT_FALSE(bid_depth.empty());
ASSERT_FALSE(ask_depth.empty());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}