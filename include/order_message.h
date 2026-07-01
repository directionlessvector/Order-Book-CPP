#pragma once
#include <cstdint>

// This is the exact byte layout that goes over the wire (UDP) later.
// pack(push, 1) removes compiler padding so sizeof(OrderMessage) is
// predictable and a raw network buffer can be cast directly into this type.
#pragma pack(push, 1)
struct OrderMessage {
    uint64_t timestamp_ns;  // when the order was created
    uint64_t order_id;      // unique id, assigned by the sender
    uint32_t price_ticks;   // price as an integer, e.g. $50.02 -> 5002
    uint32_t quantity;      // order size
    uint8_t  side;          // 0 = buy, 1 = sell
    uint8_t  msg_type;      // 0 = new, 1 = cancel, 2 = modify
};
#pragma pack(pop)

enum Side : uint8_t { BUY = 0, SELL = 1 };
enum MsgType : uint8_t { NEW = 0, CANCEL = 1, MODIFY = 2 };

struct Trade {
    uint64_t timestamp_ns;
    uint32_t price_ticks;
    uint32_t quantity;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
};

// What actually travels through the ring buffer between threads.
// We carry the raw wire message plus the time the listener thread
// received it (T0), so the matching engine can compute end-to-end
// latency once it finishes processing (T1).
struct QueuedOrder {
    OrderMessage msg;
    uint64_t recv_timestamp_ns;
};
