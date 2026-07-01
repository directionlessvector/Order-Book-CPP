#include "order_book.h"
#include <algorithm>

std::vector<Trade> OrderBook::process(const OrderMessage& msg) {
    switch (msg.msg_type) {
        case NEW:
            return match(msg);
        case CANCEL:
            cancel(msg.order_id);
            return {};
        case MODIFY:
            // Simplest correct approach: cancel then re-add as new.
            // A real system would try to preserve queue priority on
            // quantity-down modifies; out of scope here.
            cancel(msg.order_id);
            return match(msg);
    }
    return {};
}

std::vector<Trade> OrderBook::match(const OrderMessage& incoming) {
    std::vector<Trade> trades;
    uint32_t remaining_qty = incoming.quantity;

    if (incoming.side == BUY) {
        while (remaining_qty > 0 && !sell_levels_.empty() &&
               sell_levels_.begin()->first <= incoming.price_ticks) {
            remaining_qty = matchAgainstLevel(sell_levels_, sell_levels_.begin(),
                                               incoming, remaining_qty, trades);
        }
    } else {
        while (remaining_qty > 0 && !buy_levels_.empty() &&
               buy_levels_.rbegin()->first >= incoming.price_ticks) {
            auto it = std::prev(buy_levels_.end());
            remaining_qty = matchAgainstLevel(buy_levels_, it,
                                               incoming, remaining_qty, trades);
        }
    }

    if (remaining_qty > 0) {
        RestingOrder resting{incoming.order_id, incoming.price_ticks,
                              remaining_qty, incoming.side};
        auto& book = (incoming.side == BUY) ? buy_levels_ : sell_levels_;
        book[incoming.price_ticks].push_back(resting);
        order_side_[incoming.order_id] = incoming.side;
    }

    return trades;
}

template <typename MapIt>
uint32_t OrderBook::matchAgainstLevel(std::map<uint32_t, std::deque<RestingOrder>>& book,
                                       MapIt level_it, const OrderMessage& incoming,
                                       uint32_t remaining_qty, std::vector<Trade>& trades) {
    auto& queue = level_it->second;
    while (remaining_qty > 0 && !queue.empty()) {
        RestingOrder& resting = queue.front();
        uint32_t fill_qty = std::min(remaining_qty, resting.quantity);

        Trade t;
        t.timestamp_ns = incoming.timestamp_ns;
        t.price_ticks = resting.price_ticks;
        t.quantity = fill_qty;
        if (incoming.side == BUY) {
            t.buy_order_id = incoming.order_id;
            t.sell_order_id = resting.order_id;
        } else {
            t.buy_order_id = resting.order_id;
            t.sell_order_id = incoming.order_id;
        }
        trades.push_back(t);

        remaining_qty -= fill_qty;
        resting.quantity -= fill_qty;

        if (resting.quantity == 0) {
            order_side_.erase(resting.order_id);
            queue.pop_front();
        }
    }
    if (queue.empty()) {
        book.erase(level_it);
    }
    return remaining_qty;
}

// Explicit instantiation: match() only ever calls this with a regular
// map iterator (note std::prev(buy_levels_.end()) yields an iterator,
// not a reverse_iterator -- it just walks to the last element), so we
// instantiate that one case here rather than leaving the template in
// the header for every translation unit to re-instantiate.
template uint32_t OrderBook::matchAgainstLevel<std::map<uint32_t, std::deque<RestingOrder>>::iterator>(
    std::map<uint32_t, std::deque<RestingOrder>>&,
    std::map<uint32_t, std::deque<RestingOrder>>::iterator,
    const OrderMessage&, uint32_t, std::vector<Trade>&);

void OrderBook::cancel(uint64_t order_id) {
    auto it = order_side_.find(order_id);
    if (it == order_side_.end()) return;

    auto& book = (it->second == BUY) ? buy_levels_ : sell_levels_;
    for (auto level_it = book.begin(); level_it != book.end(); ++level_it) {
        auto& queue = level_it->second;
        for (auto qit = queue.begin(); qit != queue.end(); ++qit) {
            if (qit->order_id == order_id) {
                queue.erase(qit);
                if (queue.empty()) book.erase(level_it);
                order_side_.erase(it);
                return;
            }
        }
    }
}
