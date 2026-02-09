cmake_minimum_required(VERSION 3.10)

project(OrderBook VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Create a library with the core logic
add_library(OrderBookLib
    src/Orderbook/Orderbook.cpp
    src/Orderbook/Order.cpp
)

# Expose the 'include' directory to this target and anyone who links to it
target_include_directories(OrderBookLib PUBLIC include)

# Create the main executable
add_executable(order_book main.cpp)

# Link the executable to the library
target_link_libraries(order_book PRIVATE OrderBookLib)

target_link_options(order_book PRIVATE -static)
