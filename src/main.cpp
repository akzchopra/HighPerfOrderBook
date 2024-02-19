#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <atomic>
#include <mutex>

#include "../include/order_book.h"

using namespace std::chrono;

constexpr size_t NUM_ORDERS = 1'000'000;  // 1 million orders
constexpr size_t NUM_THREADS = 8;
constexpr double PRICE_MIN = 90.0;
constexpr double PRICE_MAX = 110.0;
constexpr uint32_t QTY_MIN = 100;
constexpr uint32_t QTY_MAX = 1000;

std::atomic<size_t> processed_orders{0};
std::mutex cout_mutex;

void print_progress(size_t current, double rate) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "\rProcessed: " << current
              << " orders, Rate: " << std::fixed << std::setprecision(2)
              << rate << " orders/sec" << std::flush;
}

void generate_orders(OrderBook<double>& book, size_t num_orders, size_t thread_id) {
    try {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(PRICE_MIN, PRICE_MAX);
        std::uniform_int_distribution<> qty_dist(QTY_MIN, QTY_MAX);
        std::uniform_int_distribution<> side_dist(0, 1);

        for (size_t i = 0; i < num_orders; ++i) {
            Side side = side_dist(gen) == 0 ? Side::BUY : Side::SELL;
            double price = price_dist(gen);
            uint32_t quantity = qty_dist(gen);
            std::string id = "ORD_" + std::to_string(thread_id) + "_" + std::to_string(i);

            if (book.add_limit_order(side, price, quantity, id)) {
                size_t current = processed_orders.fetch_add(1) + 1;

                if (current % 10000 == 0) {  // Update progress every 10k orders
                    auto now = high_resolution_clock::now();
                    static auto start_time = now;
                    auto duration = duration_cast<milliseconds>(now - start_time).count();
                    double rate = (current * 1000.0) / duration;
                    print_progress(current, rate);
                }
            }

            // Small sleep to prevent overwhelming the system
            if (i % 1000 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Thread " << thread_id << " error: " << e.what() << std::endl;
    }
}

void run_benchmark() {
    OrderBook<double> book;
    std::vector<std::thread> threads;
    size_t orders_per_thread = NUM_ORDERS / NUM_THREADS;

    auto start = high_resolution_clock::now();

    // Launch threads
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(generate_orders, std::ref(book), orders_per_thread, i);
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Print final statistics
    std::cout << "\n\nBenchmark Results:" << std::endl;
    std::cout << "Total orders processed: " << processed_orders.load() << std::endl;
    std::cout << "Total time: " << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "Average latency: " << duration.count() / static_cast<double>(processed_orders)
              << " microseconds per order" << std::endl;

    // Show final book state
    auto [bid, ask] = book.get_best_prices();
    std::cout << "\nFinal book state:" << std::endl;
    std::cout << "Best bid: " << bid << std::endl;
    std::cout << "Best ask: " << ask << std::endl;

    // Show top levels
    std::cout << "\nTop 5 Bid Levels:" << std::endl;
    auto bid_depth = book.get_depth(Side::BUY, 5);
    for (const auto& level : bid_depth) {
        std::cout << "Price: " << level.price
                  << ", Quantity: " << level.total_quantity
                  << ", Orders: " << level.order_count << std::endl;
    }

    std::cout << "\nTop 5 Ask Levels:" << std::endl;
    auto ask_depth = book.get_depth(Side::SELL, 5);
    for (const auto& level : ask_depth) {
        std::cout << "Price: " << level.price
                  << ", Quantity: " << level.total_quantity
                  << ", Orders: " << level.order_count << std::endl;
    }
}

int main() {
    try {
        std::cout << "Starting High-Performance Order Book Benchmark\n"
                  << "=============================================\n" << std::endl;

        std::cout << "Configuration:" << std::endl;
        std::cout << "Number of orders: " << NUM_ORDERS << std::endl;
        std::cout << "Number of threads: " << NUM_THREADS << std::endl;
        std::cout << "Price range: " << PRICE_MIN << " - " << PRICE_MAX << std::endl;
        std::cout << "Quantity range: " << QTY_MIN << " - " << QTY_MAX << "\n" << std::endl;

        run_benchmark();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}