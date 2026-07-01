#include "udp_listener.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

namespace {
uint64_t nowNs() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
               high_resolution_clock::now().time_since_epoch())
        .count();
}
}

void UdpListener::run(SpscRingBuffer<QueuedOrder, config::kRingCapacity>& ring,
                       std::atomic<bool>& running) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "UdpListener: failed to create socket\n";
        return;
    }

    // Without a receive timeout, recvfrom() blocks forever, and the
    // thread would never notice `running` flip to false -- it would
    // sit there until another packet arrived. A short timeout lets
    // the loop periodically check the shutdown flag instead.
    timeval tv{0, 100 * 1000}; // 100ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "UdpListener: bind() failed on port " << port_ << "\n";
        close(sock);
        return;
    }

    std::cout << "UdpListener: listening on UDP port " << port_ << "\n";

    char buf[sizeof(OrderMessage) + 1]; // +1 to detect oversized/malformed packets
    while (running.load(std::memory_order_relaxed)) {
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n < 0) {
            continue; // timeout or interrupted; loop back and re-check running
        }
        uint64_t t0 = nowNs();

        if (static_cast<size_t>(n) != sizeof(OrderMessage)) {
            malformed_packets_++;
            continue;
        }

        QueuedOrder item;
        std::memcpy(&item.msg, buf, sizeof(OrderMessage));
        item.recv_timestamp_ns = t0;

        if (!ring.push(item)) {
            dropped_packets_++; // consumer can't keep up; counted, not silent
        }
    }

    close(sock);
}
