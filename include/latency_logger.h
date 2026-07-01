#pragma once
#include <vector>
#include <string>
#include "order_message.h"

// Owned and called only by the matching engine thread -- not shared
// across threads, so no synchronization needed here. If you later
// want multiple matching threads, this would need its own lock or
// per-thread instances merged at the end.
class LatencyLogger {
public:
    void recordLatency(uint64_t order_id, uint64_t latency_ns);
    void recordTrade(const Trade& trade);
    void recordDrop();

    // Computes percentiles and writes trades.csv, latency.csv, summary.txt
    // into the given output directory. Also prints the summary to stdout.
    void writeReport(const std::string& output_dir);

private:
    std::vector<std::pair<uint64_t, uint64_t>> latencies_; // (order_id, latency_ns)
    std::vector<Trade> trades_;
    uint64_t dropped_count_ = 0;
};
