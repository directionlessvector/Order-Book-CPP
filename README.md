# Low-Latency Order Book Simulator

A high-performance C++17 project that simulates the core data path of an electronic trading system. The project focuses on systems programming concepts including network I/O, lock-free communication, concurrent processing, custom data structures, and latency measurement.

This is **not** a trading strategy and uses **no real market data**. A standalone UDP generator produces synthetic orders that are processed through a multi-threaded matching pipeline.

---

## Features

* Synthetic UDP market data feed
* Multi-threaded processing pipeline
* Lock-free single-producer/single-consumer ring buffer
* Price-time priority order matching
* End-to-end latency measurement
* CSV export for trades and latency statistics
* Unit tests for matching engine correctness
* Clear separation between networking and matching logic

---

## Architecture

```
udp_generator (synthetic exchange)
        |
        | UDP packets (OrderMessage)
        v
+----------------------+
|   UdpListener Thread |
|   - Receives packets |
|   - Timestamps T0    |
+----------------------+
           |
           |
           v
+----------------------+
| Lock-Free SPSC Queue |
+----------------------+
           |
           |
           v
+----------------------+
| MatchingEngine Thread|
| - OrderBook          |
| - Timestamps T1      |
+----------------------+
           |
           v
+----------------------+
|   LatencyLogger      |
| trades.csv           |
| latency.csv          |
| summary.txt          |
+----------------------+
```

The networking and matching stages are intentionally separated. The listener thread focuses exclusively on receiving packets with minimal overhead, while the matching thread performs all order book operations independently. This architecture mirrors the producer-consumer pipeline commonly used in latency-sensitive systems.

---

## Building

Requirements:

* C++17 compiler
* CMake 3.10+

```bash
mkdir build
cd build

cmake ..
make
```

The build produces three executables.

| Binary            | Description                         |
| ----------------- | ----------------------------------- |
| `order_book_test` | Unit tests for the matching engine  |
| `order_book_sim`  | Main simulator                      |
| `udp_generator`   | Synthetic UDP market data generator |

---

## Running

Run the unit tests:

```bash
./build/order_book_test
```

Run the simulator using two terminals.

Terminal 1:

```bash
./build/order_book_sim 9000 5 ./output
```

Terminal 2:

```bash
./build/udp_generator 127.0.0.1 9000 10000 4
```

Arguments:

### order_book_sim

```
order_book_sim [port] [duration_seconds] [output_directory]
```

Example:

```bash
./order_book_sim 9000 5 ./output
```

### udp_generator

```
udp_generator [host] [port] [messages_per_second] [duration_seconds]
```

Example:

```bash
./udp_generator 127.0.0.1 9000 10000 4
```

---

## Output

After execution, the simulator writes:

| File          | Description                    |
| ------------- | ------------------------------ |
| `trades.csv`  | Executed trades                |
| `latency.csv` | Per-order processing latency   |
| `summary.txt` | Throughput and latency summary |

The summary includes statistics such as:

* Total messages processed
* Executed trades
* Dropped packets
* Malformed packets
* Average latency
* Median latency
* 95th percentile latency
* 99th percentile latency
* Maximum latency

---

## Design Decisions

### Integer Tick Prices

Prices are stored as integer ticks instead of floating-point values to eliminate rounding errors and provide deterministic comparisons.

Example:

```
5002  ->  $50.02
```

---

### Price-Time Priority Order Book

The matching engine follows standard price-time priority.

The initial implementation emphasizes correctness and readability by using:

```cpp
std::map<Price, std::deque<Order>>
```

This provides deterministic ordering while keeping the implementation straightforward and easy to validate.

---

### Lock-Free SPSC Ring Buffer

Communication between the networking and matching threads uses a custom lock-free Single Producer Single Consumer ring buffer.

This avoids mutex contention while matching the application's threading model.

To reduce false sharing, producer and consumer indices are aligned onto separate cache lines.

---

### Thread Separation

Networking and matching execute on separate threads.

This design ensures that:

* network I/O never waits for matching
* matching never waits for socket operations

This separation improves throughput and more closely resembles the architecture used in low-latency event processing systems.

---

### Busy-Spinning

The processing pipeline favors busy-spinning over blocking synchronization primitives to minimize latency.

This approach increases CPU utilization but reduces wake-up delays that can occur with mutexes and condition variables.

---

### UDP Transport

The simulator intentionally uses UDP to model high-throughput market data dissemination.

The listener tracks:

* malformed packets
* dropped packets due to a full queue

These metrics are included in the final run summary.

---

### Deferred Disk Writes

Trade and latency records are buffered in memory throughout execution and written to disk after processing completes.

Keeping file I/O off the hot path prevents storage latency from affecting processing performance.

---

## Project Structure

```
order-book-sim/
├── include/
│   ├── config.h
│   ├── latency_logger.h
│   ├── matching_engine.h
│   ├── order_book.h
│   ├── order_message.h
│   ├── ring_buffer.h
│   └── udp_listener.h
│
├── src/
│   ├── latency_logger.cpp
│   ├── main.cpp
│   ├── matching_engine.cpp
│   ├── order_book.cpp
│   ├── udp_generator.cpp
│   └── udp_listener.cpp
│
├── test/
│   └── order_book_test.cpp
│
├── CMakeLists.txt
└── README.md
```

---

## Current Limitations

* No CPU affinity or thread pinning
* `MODIFY` operations are implemented as cancel followed by reinsert, which does not preserve queue priority
* Single-instrument order book
* UDP market data simulation only (no order-entry protocol)
* Occasional latency outliers caused by operating system scheduling

---

## Future Improvements

* Cache-friendly order book implementation
* CPU affinity and real-time scheduling
* Multi-instrument support
* Market data replay from recorded feeds
* Performance benchmarking under higher message rates
* Extended latency analytics and visualization

---

## Technologies

* C++17
* POSIX sockets
* CMake
* Lock-free programming
* Multi-threading
* STL
* UDP networking

---

## Learning Objectives

This project demonstrates practical experience with:

* concurrent programming
* lock-free data structures
* network programming
* systems-level C++
* low-latency software design
* order matching algorithms
* latency measurement and performance analysis
