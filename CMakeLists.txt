cmake_minimum_required(VERSION 3.20)
project(HighPerformanceOrderBook VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 as required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# M1-specific flags
add_compile_options(
        -Wall
        -Wextra
        -g
        -O1
        -mcpu=apple-m1
        -fsanitize=address
        -fno-omit-frame-pointer
)
add_link_options(-fsanitize=address)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Create interface library
add_library(order_book INTERFACE)
target_include_directories(order_book INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Create main executable
add_executable(order_book_main src/main.cpp)
target_link_libraries(order_book_main PRIVATE order_book)

# Enable testing
enable_testing()
add_subdirectory(tests)