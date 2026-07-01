#include "matching_engine.h"
#include "config.h"

// MatchingEngine is a template (so it can be reused with a different
// ring buffer capacity in tests vs. production), but the project only
// ever instantiates it with one capacity, defined in config.h. We
// explicitly instantiate that one case here so the implementation
// lives in a .cpp file rather than being re-compiled in every
// translation unit that includes the header.
template class MatchingEngine<config::kRingCapacity>;
