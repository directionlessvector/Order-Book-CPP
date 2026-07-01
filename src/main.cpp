// Entry point for the live simulator: starts the UDP listener thread
// and the matching engine thread, lets them run for a configured
// duration, then shuts down cleanly and writes the latency/trade report.
//
// Run udp_generator (a separate process) pointed at this program's
// port while this is running to actually feed it data -- see README.md.
//
// Usage: order_book_sim [port] [duration_sec] [output_dir]
#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include "config.h"
#include "ring_buffer.h"
#include "udp_listener.h"
#include "matching_engine.h"
#include "latency_logger.h"

int main(int argc, char** argv) {
    uint16_t port = (argc > 1) ? static_cast<uint16_t>(std::stoi(argv[1])) : config::kDefaultPort;
    int duration_sec = (argc > 2) ? std::stoi(argv[2]) : 5;
    std::string output_dir = (argc > 3) ? argv[3] : ".";

    std::filesystem::create_directories(output_dir);

    static SpscRingBuffer<QueuedOrder, config::kRingCapacity> ring;
    LatencyLogger logger;
    std::atomic<bool> running{true};

    UdpListener listener(port);
    MatchingEngine<config::kRingCapacity> engine(ring, logger, running);

    std::cout << "Starting simulator: port=" << port
              << " duration=" << duration_sec << "s output_dir=" << output_dir << "\n";

    std::thread listener_thread([&]() { listener.run(ring, running); });
    std::thread matcher_thread([&]() { engine.run(); });

    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    std::cout << "Shutting down...\n";
    running.store(false, std::memory_order_relaxed);

    listener_thread.join();
    matcher_thread.join();

    std::cout << "Listener dropped packets (ring full): " << listener.droppedPackets() << "\n";
    std::cout << "Listener malformed packets:           " << listener.malformedPackets() << "\n";

    logger.writeReport(output_dir);

    std::cout << "Report written to " << output_dir << "/{trades,latency,summary}.*\n";
    return 0;
}
