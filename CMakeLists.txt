cmake_minimum_required(VERSION 3.10)

project(OrderBook VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Output all executables to the 'bin' folder inside the build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

enable_testing()

# Add subdirectory - Orderbook must be first so the library target exists
add_subdirectory(Orderbook)
add_subdirectory(test)
