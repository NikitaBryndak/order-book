cmake_minimum_required(VERSION 3.10)

set(GOOGLETEST_VERSION 1.11.0)
add_subdirectory(googletest)

# Create the test executable
add_executable(OrderBookTests
    OrderBookTests.cpp
)

# Link against GTest and the main project library
target_link_libraries(OrderBookTests
    PRIVATE
    gtest_main
    OrderBookLib
)

target_link_options(OrderBookTests PRIVATE -static)

include(GoogleTest)
gtest_discover_tests(OrderBookTests)