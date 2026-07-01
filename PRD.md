# PRD: Low-Latency Order Book Simulator

## 1. Summary

A self-contained C++ system that simulates the core plumbing of an electronic trading platform: a synthetic market data feed sent over UDP, a multi-threaded pipeline that ingests and matches orders, and latency instrumentation that measures how fast the system processes each one. Built as a portfolio project to demonstrate systems-level C++ skills relevant to low-latency trading infrastructure roles, not as a real trading or financial product.

## 2. Problem statement

Trading infrastructure roles require proof of specific, hard-to-fake skills: custom data structures, network programming over TCP/UDP, multi-threaded/concurrent design, and an obsession with tail latency rather than average-case performance. Generic coding projects (web apps, CRUD tools) don't demonstrate any of this. This project exists to produce a concrete, measurable, defensible artifact for that purpose.

## 3. Goals

- Build a working limit order book with correct matching logic.
- Demonstrate a custom, domain-optimized data structure (not just `std::map`).
- Demonstrate UDP network programming with a hand-rolled binary wire format.
- Demonstrate multi-threaded design using a lock-free ring buffer between threads.
- Produce a measured, reproducible latency report (p50/p99/p999).
- Produce a README that documents design decisions clearly enough to drive a technical interview conversation.

## 4. Non-goals

- Real exchange connectivity, real market data, or real money.
- Trading strategy, alpha generation, or predictive modeling — out of scope entirely.
- A persistent database or live storage layer on the hot path.
- A UI or dashboard (a CSV + simple latency histogram is sufficient).
- Production-grade fault tolerance, authentication, or security hardening.

## 5. Users

A single user: the project author, using this as a take-home artifact to discuss in interviews for trading-systems/low-latency C++ developer roles. No external users or real-world deployment.

## 6. System overview

```
Market data generator (UDP sender, fake exchange)
        |
        v
Listener thread (UDP recv, parse, timestamp T0)
        |
        v
SPSC ring buffer (lock-free handoff)
        |
        v
Matching engine thread (order book, timestamp T1)
        |
        v
Latency logger + trade log (CSV output)
```

## 7. Functional requirements

| ID | Requirement |
|----|-------------|
| FR1 | System shall generate synthetic order messages (new/cancel/modify) at a configurable rate and send them over UDP. |
| FR2 | System shall receive UDP packets, parse them into typed order messages, and timestamp them on receipt. |
| FR3 | System shall maintain a buy-side and sell-side order book, each queryable for best price in O(1). |
| FR4 | System shall match crossing orders (buy price >= sell price) and generate trade records. |
| FR5 | System shall support order cancellation by order ID in O(1) average case. |
| FR6 | System shall pass resting (unmatched) orders between threads via a lock-free single-producer/single-consumer ring buffer. |
| FR7 | System shall timestamp each order at receipt and at processing completion, and compute end-to-end latency per order. |
| FR8 | System shall write all completed trades to a CSV file. |
| FR9 | System shall write a latency summary (p50, p99, p999, throughput) to console and/or file at the end of a run. |

## 8. Data model / message schema

Wire format (UDP payload), fixed-size, packed struct, no padding:

| Field | Type | Description |
|---|---|---|
| timestamp_ns | uint64 | Order creation time, nanoseconds |
| order_id | uint64 | Unique order identifier |
| price_ticks | uint32 | Price as integer ticks (never float) |
| quantity | uint32 | Order size |
| side | uint8 | 0 = buy, 1 = sell |
| msg_type | uint8 | 0 = new, 1 = cancel, 2 = modify |

In-memory structures (the "database" for this system):

- **Order book (per side):** initially `std::map<price_ticks, std::deque<Order>>`; optimized to a flat array indexed by `price - min_price`, each slot an intrusive linked list, for O(1) best-price access and insert/cancel.
- **Order lookup index:** `std::unordered_map<order_id, OrderLocation>` for O(1) cancel/modify lookups.
- **Ring buffer:** fixed-size circular array of `OrderMessage`, atomic head/tail indices, no mutex.

Persisted output (off the hot path, written after processing, not part of live system):

- `trades.csv`: timestamp, price, quantity, buy_order_id, sell_order_id
- `latency.csv`: order_id, latency_ns
- `summary.txt`: p50/p99/p999 latency, total trades, throughput

No live database is used. A database (e.g. SQLite) is optional and out-of-band, purely for ad hoc querying of the CSV output after a run.

## 9. Non-functional requirements

- **Latency:** order processing latency (T1 - T0) should be measured and reported; no fixed target, but the report must surface p99/p999, not just average.
- **Throughput:** system should sustain at least 10,000 synthetic messages/sec without dropping packets (UDP, so drops are possible — count and report them).
- **Determinism/correctness:** matching logic must be unit-testable independent of networking/threading, with known input sequences producing known trade output.
- **Portability:** Linux-only target (matches the JD's emphasis on Linux environments); no Windows/macOS support required.

## 10. Milestones / roadmap

| Phase | Deliverable | Est. time |
|---|---|---|
| 1 | Message schema + order book (std::map version) + unit tests | 0.5 day |
| 2 | Custom flat-array order book (optimization pass) | 0.5 day |
| 3 | UDP generator + UDP listener (tested in isolation) | 0.5 day |
| 4 | Ring buffer + multi-threaded wiring (listener -> matcher) | 0.5 day |
| 5 | Latency logging + CSV output | 0.5 day |
| 6 | README, architecture diagram, latency histogram, polish | 0.5 day |

Total: ~2 days (matches scope constraint).

## 11. Success criteria

- Order book produces correct trades against a hand-verified test suite.
- System runs end-to-end: generator -> network -> ring buffer -> matcher -> CSV output, with no crashes over a sustained run (e.g. 1M messages).
- Latency report shows p50/p99/p999 with p999 clearly distinguished from p50 (proving the report isn't just an average masquerading as a percentile breakdown).
- README clearly explains: why integer ticks not floats, why a flat array over std::map, why a lock-free ring buffer over a mutex queue, and what the measured latency numbers were.

## 12. Risks / open questions

- **Risk:** UDP is unreliable; packet loss under load needs to be counted, not silently ignored, or the project looks naive about networking.
- **Risk:** Naive lock-free ring buffer implementations are easy to get subtly wrong (memory ordering bugs); should be tested under contention, not just functionally.
- **Open question:** whether to add a TCP-based order-entry path as a stretch goal to demonstrate both protocols, time permitting.
