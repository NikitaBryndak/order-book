cmake_minimum_required(VERSION 3.10)

project(OrderBook VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Create a library with Orderbook
add_library(OrderBookLib
src/Orderbook/Orderbook.cpp
src/Orderbook/Order.cpp
src/Trader/Trader.cpp)

# Expose the 'include' directory to this target and anyone who links to it
target_include_directories(OrderBookLib PUBLIC include)

add_executable(order_book main.cpp)
target_link_libraries(order_book PRIVATE OrderBookLib)
target_link_options(order_book PRIVATE -static)

# ---- Qt6 GUI executable ----
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)

  # Build a UI-enabled copy of the library with candlestick/snapshot support
  add_library(OrderBookLibUI
    src/Orderbook/Orderbook.cpp
    src/Orderbook/Order.cpp
    src/Trader/Trader.cpp)
  target_include_directories(OrderBookLibUI PUBLIC include)
  target_compile_definitions(OrderBookLibUI PUBLIC OB_ENABLE_UI)

  add_executable(order_book_qt
    main_qt.cpp
    include/Orderbook/OrderbookUI.hpp)
  target_link_libraries(order_book_qt PRIVATE OrderBookLibUI Qt6::Widgets)
  target_include_directories(order_book_qt PRIVATE include)
else()
  message(STATUS "Qt6 not found – skipping order_book_qt target")
endif()
