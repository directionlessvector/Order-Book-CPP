#pragma once
#include <atomic>
#include "order_message.h"
#include "ring_buffer.h"
#include "config.h"

// Runs on its own thread. Opens a UDP socket, receives raw packets,
// validates and casts them into OrderMessage, stamps the receive
// time (T0), and pushes the result into the ring buffer for the
// matching engine thread to consume.
//
// UDP gives no delivery guarantee, so packet loss is expected under
// load, not a bug -- we count it rather than pretend it can't happen.
class UdpListener {
public:
    explicit UdpListener(uint16_t port) : port_(port) {}

    // Blocks until running becomes false. Intended to run on its own
    // std::thread.
    void run(SpscRingBuffer<QueuedOrder, config::kRingCapacity>& ring,
              std::atomic<bool>& running);

    uint64_t droppedPackets() const { return dropped_packets_; }
    uint64_t malformedPackets() const { return malformed_packets_; }

private:
    uint16_t port_;
    uint64_t dropped_packets_ = 0;   // ring buffer was full
    uint64_t malformed_packets_ = 0; // wrong size, discarded
};
