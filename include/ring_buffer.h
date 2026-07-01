#pragma once
#include <atomic>
#include <array>
#include <cstddef>

// Single-producer, single-consumer lock-free ring buffer.
//
// Why no mutex: the listener thread (producer) and matching engine
// thread (consumer) each touch this from exactly one thread, so we
// don't need general-purpose mutual exclusion -- we need a correct
// handoff protocol between exactly two threads, which is what the
// acquire/release pairing below provides.
//
// Why it's correct without locks:
//   - producer only ever writes head_, only ever reads tail_
//   - consumer only ever writes tail_, only ever reads head_
//   - release on the writer + acquire on the reader of the same
//     atomic creates a happens-before edge, so by the time the
//     consumer observes a new head_ value, it is guaranteed to also
//     see the slot data the producer wrote just before publishing it.
//
// Capacity must be a power of two so index wraparound can use a
// bitmask instead of a modulo.
template <typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    // Returns false if the buffer is full (caller decides: spin, drop, or count it).
    bool push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // full
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Returns false if the buffer is empty.
    bool pop(T& out) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        out = buffer_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    size_t capacity() const { return Capacity; }

private:
    static constexpr size_t kMask = Capacity - 1;
    std::array<T, Capacity> buffer_{};

    // alignas(64) puts head_ and tail_ on separate cache lines.
    // Without this, the producer constantly writing head_ and the
    // consumer constantly writing tail_ would "false share" the same
    // cache line and ping-pong it between cores on every update --
    // a real performance bug, not just a style choice.
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};
