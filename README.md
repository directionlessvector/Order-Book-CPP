# Low-Latency Order Book Simulator

A small C++ system that simulates the core plumbing of an electronic trading
platform: a synthetic UDP market data feed, a multi-threaded ingestion and
matching pipeline, and end-to-end latency measurement. Built as a portfolio
project for low-latency/trading-systems C++ roles — see `PRD.md` for full
requirements.

This is **not** a trading strategy and uses **no real market data**. The
"exchange" (`udp_generator`) makes up random orders. The goal is to
demonstrate systems-level engineering: network I/O, concurrency, custom data
structures, and tail-latency measurement.

## Architecture

```
udp_generator (separate process, fake exchange)
        |  UDP packets, OrderMessage wire format
        v
UdpListener thread  -- stamps T0 (receive time)
        |
        v
SpscRingBuffer<QueuedOrder>  -- lock-free handoff
        |
        v
MatchingEngine thread  -- OrderBook, stamps T1 (processed time)
        |
        v
LatencyLogger  -- trades.csv, latency.csv, summary.txt
```

Two threads, one lock-free queue between them. The listener thread's only
job is to get packets off the network as fast as possible; it never blocks
on book logic. The matching thread's only job is to process orders
correctly; it never blocks on network I/O. This separation is the standard
shape of real low-latency systems, not specific to trading.

## Build & run

Requires a C++17 compiler and CMake 3.10+.

```bash
mkdir build && cd build
cmake .. && make
```

This produces three binaries:

| Binary | Purpose |
|---|---|
| `order_book_test` | Unit tests for matching logic, no networking/threads |
| `order_book_sim` | The live simulator (listener + matcher threads) |
| `udp_generator` | Standalone fake exchange, sends synthetic orders over UDP |

Run the tests:
```bash
./build/order_book_test
```

Run the live pipeline (two terminals):
```bash
# terminal 1
./build/order_book_sim 9000 5 ./output

# terminal 2, started shortly after
./build/udp_generator 127.0.0.1 9000 10000 4
```

`order_book_sim [port] [duration_sec] [output_dir]` listens for
`duration_sec` seconds then writes `trades.csv`, `latency.csv`, and
`summary.txt` to `output_dir`.

`udp_generator [host] [port] [messages_per_sec] [duration_sec]` sends
synthetic orders at the given rate.

## Design decisions (the part worth discussing in an interview)

**Integer ticks, not floating point, for price.** Floats accumulate
rounding error and comparisons become fuzzy. Exchanges represent price as
an integer multiple of the minimum tick size; `5002` means $50.02 given a
$0.01 tick. This is a small detail that signals real exposure to how
trading systems actually represent money.

**`std::map`-based order book first, before any optimization.** Phase 1
(see `PRD.md`) deliberately uses `std::map<price, deque<Order>>` — correct
and easy to verify against unit tests — before any performance work. The
flat-array, intrusive-linked-list version described in the PRD as a
stretch goal is *not yet implemented* here; it's the natural next session.
Building it before the simple version was proven correct would make bugs
in the optimization indistinguishable from bugs in the logic.

**Lock-free SPSC ring buffer instead of a mutex-protected queue.** The
listener thread (producer) and matching thread (consumer) are exactly one
each, which is precisely the case `SpscRingBuffer` is designed for — using
a general-purpose mutex queue here would be paying lock overhead for a
guarantee (multi-producer/multi-consumer safety) the system doesn't need.
`head_`/`tail_` are on separate cache lines (`alignas(64)`) to avoid false
sharing between the two threads.

**Busy-spin instead of blocking on the ring buffer.** Both the matcher's
consume loop and the listener's `recvfrom` avoid blocking primitives where
possible, trading CPU usage for lower latency. This is a real, named
trade-off: a busy-spinning thread burns a full core continuously. In a
production system you'd usually pin these threads to dedicated cores
(`pthread_setaffinity_np` — not yet done here) so they don't contend with
the rest of the OS for CPU time.

**UDP, with explicit drop/malformed counters.** UDP gives no delivery
guarantee. Rather than assuming packets always arrive, `UdpListener`
counts malformed packets (wrong size) and ring-buffer-full drops
separately, and the run summary reports both. A system that doesn't count
its own data loss isn't trustworthy.

**No database on the hot path.** Trade and latency records are buffered in
memory during the run and flushed to CSV only after processing completes.
A live database write per order would dominate the latency budget. See
`PRD.md` section 8 for the full reasoning.

## Known limitations / honest next steps

- The flat-array order book optimization (PRD Phase 2) isn't built yet.
- No CPU pinning/affinity — the threads compete with the rest of the OS.
- `MODIFY` is implemented as cancel + re-add, which loses queue priority;
  a more faithful implementation would preserve priority on quantity-down
  modifies.
- No TCP order-entry path (mentioned as a stretch goal in the PRD) — only
  the UDP market-data side is built.
- The max observed latency in test runs includes occasional multi-hundred-
  microsecond outliers, almost certainly from OS thread scheduling jitter
  on the busy-spin loops rather than the matching logic itself — worth
  investigating with `chrt`/real-time scheduling priority as a next step.

## File layout

```
order-book-sim/
  include/order_message.h   wire format struct + Trade + QueuedOrder
  include/order_book.h      order book class declaration
  include/ring_buffer.h     lock-free SPSC ring buffer
  include/matching_engine.h matching engine thread (template)
  include/latency_logger.h  percentile computation + CSV/summary output
  include/udp_listener.h    UDP listener thread
  include/config.h          shared constants (ring capacity, default port)
  src/order_book.cpp        matching logic implementation
  src/matching_engine.cpp   explicit template instantiation
  src/udp_listener.cpp      socket setup + receive loop
  src/udp_generator.cpp     standalone fake exchange (has its own main)
  src/latency_logger.cpp    percentile calc + report writing
  src/main.cpp              wires listener + matcher threads together
  test/order_book_test.cpp  unit tests for matching logic
  CMakeLists.txt
  PRD.md
  README.md
```
