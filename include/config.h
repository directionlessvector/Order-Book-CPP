#pragma once
#include <cstddef>
#include <cstdint>

// Shared constants. Centralized here so the ring buffer capacity used
// by main.cpp, matching_engine's explicit instantiation, and the test
// harness all agree -- a mismatch here would be a silent, confusing bug.
namespace config {
constexpr size_t kRingCapacity = 1 << 16; // must be a power of two
constexpr uint16_t kDefaultPort = 9000;
}
