# Order Book

A high-performance, thread-safe implementation of a Limit Order Book in C++20. This project demonstrates a matching engine capable of handling limit orders (Buy/Sell), cancellations, and automatic trade execution with Price/Time priority.

## Features

*   **Limit Orders:** Support for Buy and Sell limit orders.
*   **Automatic Matching:** Orders are matched automatically based on Price/Time priority.
*   **Thread Safety:** Fully thread-safe operations allowing concurrent order submission and cancellation.
*   **Lazy Cancellation:** Implements "ghost orders" to optimize cancellation performance by deferring cleanup until necessary.
*   **Performance:** Includes built-in benchmarks to measure Operations Per Second (OPS).
*   **Testing:** Comprehensive unit test suite using Google Test.

## Prerequisites

*   C++20 compatible compiler (GCC, Clang, MSVC)
*   CMake 3.10 or higher

## Build Instructions

1.  Clone the repository:
    ```bash
    git clone https://github.com/NikitaBryndak/order-book.git
    cd order-book
    ```

2.  Create a build directory and compile:
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

## Running the Application

### Benchmark Demo
The main application runs a multi-threaded benchmark to demonstrate throughput.

```bash
./bin/order_book
```
*Note: On Windows, the executable will be `.\bin\order_book.exe`.*

### Running Tests
The project uses Google Test for unit testing.

```bash
./bin/OrderBookTests
```
*Note: On Windows, the executable will be `.\bin\OrderBookTests.exe`.*

## Performance

The system includes performance tests to measure throughput under load.

*   **Single Threaded:** ~725k OPS
*   **Multi-Threaded (4 threads):** ~420k OPS

*Performance metrics may vary based on hardware.*

## Project Structure

*   `Orderbook/`: Source code for the Order Book library and main application.
    *   `include/`: Header files (`Orderbook.hpp`, `Order.hpp`).
    *   `src/`: Implementation files.
*   `test/`: Unit tests using Google Test.
