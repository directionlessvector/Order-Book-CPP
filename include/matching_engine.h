#pragma once
#include <atomic>
#include <chrono>
#include "order_book.h"
#include "ring_buffer.h"
#include "latency_logger.h"

// Runs on its own thread. Pulls QueuedOrder items off the ring buffer
// (pushed there by the UDP listener thread), feeds each into the
// order book, and records end-to-end latency: from the moment the
// listener thread received the packet (T0) to the moment matching
// finished for that message (T1).
template <size_t RingCapacity>
class MatchingEngine {
public:
    MatchingEngine(SpscRingBuffer<QueuedOrder, RingCapacity>& ring,
                   LatencyLogger& logger,
                   std::atomic<bool>& running)
        : ring_(ring), logger_(logger), running_(running) {}

    // Intended to be run via std::thread. Spins on the ring buffer
    // rather than blocking, since blocking primitives (condition
    // variables, mutex waits) introduce latency we're trying to avoid.
    // This trades CPU usage for latency -- a deliberate, named
    // trade-off, not an oversight.
    void run() {
        QueuedOrder item;
        while (running_.load(std::memory_order_relaxed)) {
            if (!ring_.pop(item)) {
                continue; // busy-spin; nothing to process yet
            }
            std::vector<Trade> trades = book_.process(item.msg);

            uint64_t t1 = nowNs();
            uint64_t latency_ns = t1 - item.recv_timestamp_ns;
            logger_.recordLatency(item.msg.order_id, latency_ns);

            for (const auto& trade : trades) {
                logger_.recordTrade(trade);
            }
        }
        // Drain whatever's left in the ring buffer after shutdown is
        // signalled, so a stop doesn't silently discard queued work.
        while (ring_.pop(item)) {
            std::vector<Trade> trades = book_.process(item.msg);
            uint64_t t1 = nowNs();
            logger_.recordLatency(item.msg.order_id, t1 - item.recv_timestamp_ns);
            for (const auto& trade : trades) logger_.recordTrade(trade);
        }
    }

private:
    OrderBook book_;
    SpscRingBuffer<QueuedOrder, RingCapacity>& ring_;
    LatencyLogger& logger_;
    std::atomic<bool>& running_;

    static uint64_t nowNs() {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(
                   high_resolution_clock::now().time_since_epoch())
            .count();
    }
};
