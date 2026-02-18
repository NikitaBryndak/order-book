# High-Performance Order Book

A low-latency, thread-safe implementation of a Limit Order Book (LOB) written in C++20. This project demonstrates a matching engine capable of handling millions of operations per second, supporting Limit Buy/Sell orders, cancellations, and modifications with Price/Time priority.

## 🚀 Key Features

*   **High Performance:** Capable of processing over **3 million operations per second** on standard hardware.
*   **Lock-Free Command Queue:** Uses a highly optimized, cache-friendly `RingBuffer` for non-blocking communication between order producers and the matching engine.
*   **Memory Pooling:** Custom `OrderPool` reduces heap allocation overhead during runtime, ensuring stable latency.
*   **Price/Time Priority:** Standard FIFO matching algorithm.
*   **Lazy Cancellation:** Efficient order cancellation strategy ("ghost orders") to minimize O(n) traversals in linked lists.
*   **Thread Safety:** Supports concurrent order submission from multiple threads.

## 🛠️ Technology Stack

*   **Language:** C++20
*   **Build System:** CMake (3.10+)
*   **Testing:** Google Test (GTest)
*   **Platform:** Cross-platform (Windows, Linux, macOS)

## 🏗️ Architecture

The system consists of two main components running on separate threads to maximize throughput:

1.  **Request Handling (Producers):** Multiple threads can submit `OrderRequest` objects (`Add`, `Cancel`, `Modify`). These requests are pushed into a lock-free Ring Buffer.
2.  **Matching Engine (Consumer):** A dedicated single thread polls the Ring Buffer, processes requests sequentially, and executes matches. This design avoids heavy locking on the critical path.
3.  **Memory Management:** Pre-allocated memory pools prevent expensive `new`/`delete` calls during the trading session.

## 📦 Build Instructions

### Prerequisites
*   C++20 compatible compiler (GCC, Clang, MSVC)
*   CMake 3.10 or higher

```bash
git clone <repo>
cd order-book
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## ⚡ Running Benchmarks & Tests

### Unit Tests
The project includes a comprehensive test suite using Google Test.

```bash
# From the build directory
./bin/OrderBookTests
```
*Note: On Windows, use `.\bin\OrderBookTests.exe` (and ensure your compiler's bin directory is in PATH).*

### Performance Benchmarks
The tests include a built-in benchmark scenario simulating real-world traffic with 10 concurrent producer threads.

**Sample Output (10 Threads, 10M Operations):**
```text
[ RUN      ] OrderBookTest.Benchmark_RealWorldScenario
Starting Multi-Threaded Benchmark (10 producers, 10000000 ops)...
Processed 10000000 ops in 3.24291 s
Throughput: 3083649 ops/sec
[       OK ] OrderBookTest.Benchmark_RealWorldScenario
```

## 📂 Project Structure

```
OrderBook/
├── Orderbook/          # Core library source code
│   ├── include/        # Header files (Orderbook.hpp, RingBuffer.hpp, etc.)
│   └── src/            # Implementation files
├── test/               # Unit tests and benchmarks
├── build/              # Compiled binaries (generated)
└── CMakeLists.txt      # Root build configuration
```
