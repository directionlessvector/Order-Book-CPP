// Standalone program: the "fake exchange." Generates synthetic
// new/cancel order messages and fires them over UDP at a configurable
// rate. Run this in a separate terminal from the main simulator.
//
// Usage: udp_generator [host] [port] [messages_per_sec] [duration_sec]
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include "order_message.h"
#include "config.h"

namespace {
uint64_t nowNs() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
               high_resolution_clock::now().time_since_epoch())
        .count();
}
}

int main(int argc, char** argv) {
    std::string host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t port = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : config::kDefaultPort;
    int messages_per_sec = (argc > 3) ? std::stoi(argv[3]) : 10000;
    int duration_sec = (argc > 4) ? std::stoi(argv[4]) : 5;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid host: " << host << "\n";
        return 1;
    }

    // Synthetic order generation: prices random-walk around a midpoint
    // instead of being uniformly random, so the book sees realistic
    // clustering and actually produces a meaningful number of trades
    // rather than mostly-resting, never-crossing orders.
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> price_walk(-5, 5);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    uint32_t mid_price = 5000; // $50.00 in ticks (1 tick = $0.01)
    uint64_t order_id = 1;
    std::vector<uint64_t> live_order_ids; // for occasional cancels

    const auto interval = std::chrono::nanoseconds(1'000'000'000 / messages_per_sec);
    const auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);

    uint64_t sent = 0;
    std::cout << "udp_generator: sending to " << host << ":" << port
              << " at ~" << messages_per_sec << " msg/sec for " << duration_sec << "s\n";

    while (std::chrono::steady_clock::now() < end_time) {
        auto loop_start = std::chrono::steady_clock::now();

        OrderMessage msg{};
        msg.timestamp_ns = nowNs();

        // Roughly 1 in 10 messages is a cancel of a previously-sent
        // live order, if we have one; otherwise it's a new order.
        bool send_cancel = !live_order_ids.empty() && (order_id % 10 == 0);
        if (send_cancel) {
            msg.order_id = live_order_ids.back();
            live_order_ids.pop_back();
            msg.msg_type = CANCEL;
            msg.price_ticks = 0;
            msg.quantity = 0;
            msg.side = BUY; // side is unused by cancel logic; harmless placeholder
        } else {
            mid_price = static_cast<uint32_t>(
                std::max(1, static_cast<int>(mid_price) + price_walk(rng)));
            msg.order_id = order_id++;
            msg.msg_type = NEW;
            msg.price_ticks = mid_price;
            msg.quantity = qty_dist(rng);
            msg.side = static_cast<uint8_t>(side_dist(rng));
            live_order_ids.push_back(msg.order_id);
            if (live_order_ids.size() > 1000) live_order_ids.erase(live_order_ids.begin());
        }

        sendto(sock, &msg, sizeof(msg), 0,
               reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        sent++;

        auto target = loop_start + interval;
        std::this_thread::sleep_until(target);
    }

    std::cout << "udp_generator: sent " << sent << " messages\n";
    close(sock);
    return 0;
}
