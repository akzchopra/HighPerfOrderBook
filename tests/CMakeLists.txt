find_package(GTest REQUIRED)
include(GoogleTest)

# Create test executable
add_executable(test_order_book test_order_book.cpp)
target_link_libraries(test_order_book
        PRIVATE
        order_book
        GTest::gtest_main
)

# Enable testing
gtest_discover_tests(test_order_book)