cmake_minimum_required(VERSION 3.10)

project(OrderBook VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the header files directory
include_directories(include)

# Define source files
set(SOURCES
    src/Orderbook.cpp
    src/Order.cpp
    main.cpp
)

# Define header files (optional, for IDE support)
set(HEADERS
    include/Constants.hpp
    include/Order.hpp
    include/Orderbook.hpp
)

# Create the executable
# Note: Ensure one of the source files contains a 'main' function.
add_executable(OrderBook ${SOURCES} ${HEADERS})
