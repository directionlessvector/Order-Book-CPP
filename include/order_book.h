#pragma once
#include <map>
#include <deque>
#include <unordered_map>
#include <vector>
#include "order_message.h"

struct RestingOrder {
    uint64_t order_id;
    uint32_t price_ticks;
    uint32_t quantity;
    uint8_t  side;
};

// Phase 1 order book: std::map keyed by price, correctness over speed.
// See README.md for why this gets replaced by a flat-array book in
// the optimization pass, and what that trade-off actually buys you.
class OrderBook {
public:
    std::vector<Trade> process(const OrderMessage& msg);

    bool hasBestBid() const { return !buy_levels_.empty(); }
    bool hasBestAsk() const { return !sell_levels_.empty(); }
    uint32_t bestBid() const { return buy_levels_.rbegin()->first; }
    uint32_t bestAsk() const { return sell_levels_.begin()->first; }

private:
    std::map<uint32_t, std::deque<RestingOrder>> buy_levels_;
    std::map<uint32_t, std::deque<RestingOrder>> sell_levels_;
    std::unordered_map<uint64_t, uint8_t> order_side_;

    std::vector<Trade> match(const OrderMessage& incoming);
    void cancel(uint64_t order_id);

    template <typename MapIt>
    uint32_t matchAgainstLevel(std::map<uint32_t, std::deque<RestingOrder>>& book,
                                MapIt level_it, const OrderMessage& incoming,
                                uint32_t remaining_qty, std::vector<Trade>& trades);
};
