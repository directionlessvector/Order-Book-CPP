cmake_minimum_required(VERSION 3.10)
project(order_book_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

include_directories(include)
find_package(Threads REQUIRED)

# Core engine, shared by the test binary and the live simulator.
add_library(order_book_core
    src/order_book.cpp
    src/latency_logger.cpp
    src/matching_engine.cpp
)

# order_book_test: pure logic tests, no networking, no threads needed.
add_executable(order_book_test test/order_book_test.cpp)
target_link_libraries(order_book_test order_book_core)

# order_book_sim: the live multi-threaded simulator (listener + matcher).
add_executable(order_book_sim src/main.cpp src/udp_listener.cpp)
target_link_libraries(order_book_sim order_book_core Threads::Threads)

# udp_generator: standalone "fake exchange" process.
add_executable(udp_generator src/udp_generator.cpp)
target_link_libraries(udp_generator Threads::Threads)
