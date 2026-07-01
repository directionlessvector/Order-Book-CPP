#include <cassert>
#include <iostream>
#include "order_book.h"

OrderMessage makeOrder(uint64_t id, uint32_t price, uint32_t qty, uint8_t side,
                        uint8_t type = NEW) {
    return OrderMessage{0, id, price, qty, side, type};
}

void test_resting_order_no_match() {
    OrderBook book;
    auto trades = book.process(makeOrder(1, 5000, 100, BUY));
    assert(trades.empty());
    assert(book.hasBestBid());
    assert(book.bestBid() == 5000);
    std::cout << "PASS: resting order with no match\n";
}

void test_simple_match() {
    OrderBook book;
    book.process(makeOrder(1, 5000, 100, BUY));   // resting buy at 50.00
    auto trades = book.process(makeOrder(2, 5000, 100, SELL)); // crosses
    assert(trades.size() == 1);
    assert(trades[0].price_ticks == 5000);
    assert(trades[0].quantity == 100);
    assert(trades[0].buy_order_id == 1);
    assert(trades[0].sell_order_id == 2);
    assert(!book.hasBestBid());
    std::cout << "PASS: simple crossing match\n";
}

void test_partial_fill() {
    OrderBook book;
    book.process(makeOrder(1, 5000, 100, BUY));
    auto trades = book.process(makeOrder(2, 5000, 40, SELL)); // only partially fills
    assert(trades.size() == 1);
    assert(trades[0].quantity == 40);
    assert(book.hasBestBid());
    assert(book.bestBid() == 5000); // remainder of order 1 still resting
    std::cout << "PASS: partial fill leaves remainder resting\n";
}

void test_price_priority() {
    OrderBook book;
    // Two resting sells: cheaper one should fill first even though added second.
    book.process(makeOrder(1, 5010, 50, SELL));
    book.process(makeOrder(2, 5005, 50, SELL));
    auto trades = book.process(makeOrder(3, 5010, 50, BUY));
    assert(trades.size() == 1);
    assert(trades[0].sell_order_id == 2); // the cheaper resting order, not order 1
    std::cout << "PASS: price priority respected\n";
}

void test_cancel() {
    OrderBook book;
    book.process(makeOrder(1, 5000, 100, BUY));
    book.process(makeOrder(99, 0, 0, BUY, CANCEL)); // cancel of unrelated id: no-op
    assert(book.hasBestBid());

    OrderMessage cancel_msg{0, 1, 0, 0, BUY, CANCEL};
    book.process(cancel_msg);
    assert(!book.hasBestBid());
    std::cout << "PASS: cancel removes resting order\n";
}

int main() {
    test_resting_order_no_match();
    test_simple_match();
    test_partial_fill();
    test_price_priority();
    test_cancel();
    std::cout << "\nAll tests passed.\n";
    return 0;
}
