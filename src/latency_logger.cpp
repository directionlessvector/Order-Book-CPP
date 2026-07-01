#include "latency_logger.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>

void LatencyLogger::recordLatency(uint64_t order_id, uint64_t latency_ns) {
    latencies_.emplace_back(order_id, latency_ns);
}

void LatencyLogger::recordTrade(const Trade& trade) {
    trades_.push_back(trade);
}

void LatencyLogger::recordDrop() {
    dropped_count_++;
}

namespace {
// Nearest-rank percentile on a *sorted* vector. Simple and correct;
// not the most statistically rigorous method, but standard for this
// kind of latency reporting and easy to defend in an interview.
uint64_t percentile(const std::vector<uint64_t>& sorted_values, double p) {
    if (sorted_values.empty()) return 0;
    size_t idx = static_cast<size_t>(p * static_cast<double>(sorted_values.size()));
    if (idx >= sorted_values.size()) idx = sorted_values.size() - 1;
    return sorted_values[idx];
}
}

void LatencyLogger::writeReport(const std::string& output_dir) {
    std::ofstream trades_csv(output_dir + "/trades.csv");
    trades_csv << "timestamp_ns,price_ticks,quantity,buy_order_id,sell_order_id\n";
    for (const auto& t : trades_) {
        trades_csv << t.timestamp_ns << "," << t.price_ticks << "," << t.quantity
                    << "," << t.buy_order_id << "," << t.sell_order_id << "\n";
    }

    std::ofstream latency_csv(output_dir + "/latency.csv");
    latency_csv << "order_id,latency_ns\n";
    std::vector<uint64_t> values;
    values.reserve(latencies_.size());
    for (const auto& [order_id, latency_ns] : latencies_) {
        latency_csv << order_id << "," << latency_ns << "\n";
        values.push_back(latency_ns);
    }
    std::sort(values.begin(), values.end());

    uint64_t p50 = percentile(values, 0.50);
    uint64_t p99 = percentile(values, 0.99);
    uint64_t p999 = percentile(values, 0.999);
    uint64_t max_lat = values.empty() ? 0 : values.back();

    std::ofstream summary(output_dir + "/summary.txt");
    auto emit = [&](std::ostream& os) {
        os << "=== Run Summary ===\n";
        os << "Messages processed: " << values.size() << "\n";
        os << "Trades executed:    " << trades_.size() << "\n";
        os << "Packets dropped:    " << dropped_count_ << "\n";
        os << "Latency p50:        " << p50  << " ns\n";
        os << "Latency p99:        " << p99  << " ns\n";
        os << "Latency p999:       " << p999 << " ns\n";
        os << "Latency max:        " << max_lat << " ns\n";
    };
    emit(summary);
    emit(std::cout);
}
