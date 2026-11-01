// Kiểm thử Deque. Người phụ trách: A
#include "../src/core/Deque.h"
#include "check.h"

static void empty_at_start() {
    Deque<int> d;
    CHECK(d.empty());
    CHECK(d.size() == 0);
}

static void push_order() {
    Deque<int> d;
    d.pushBack(2);
    d.pushFront(1);
    d.pushBack(3);
    CHECK((d.toVector() == std::vector<int>{1, 2, 3}));
    CHECK(d.peekFront() == 1);
    CHECK(d.peekBack() == 3);
}

static void pop_both_ends() {
    Deque<int> d;
    for (int i = 1; i <= 3; ++i) d.pushBack(i);
    CHECK(d.popFront() == 1);
    CHECK(d.popBack() == 3);
    CHECK(d.size() == 1);
}

static void pop_last_element_resets() {
    Deque<int> d;
    d.pushBack(7);
    d.popBack();
    CHECK(d.empty());
    d.pushFront(8);                 // dùng lại được sau khi về rỗng
    CHECK(d.peekBack() == 8);
}

static void empty_throws() {
    Deque<int> d;
    bool a = false, b = false, c = false;
    try { d.popFront(); } catch (const std::underflow_error&) { a = true; }
    try { d.popBack(); } catch (const std::underflow_error&) { b = true; }
    try { d.peekFront(); } catch (const std::underflow_error&) { c = true; }
    CHECK(a && b && c);
}

static void remove_middle() {
    Deque<int> d;
    for (int i = 1; i <= 3; ++i) d.pushBack(i);
    int removed = 0;
    CHECK(d.removeFirstIf([](const int& x) { return x == 2; }, &removed));
    CHECK(removed == 2);
    CHECK((d.toVector() == std::vector<int>{1, 3}));
}

static void remove_ends_and_missing() {
    Deque<int> d;
    for (int i = 1; i <= 3; ++i) d.pushBack(i);
    CHECK(d.removeFirstIf([](const int& x) { return x == 1; }));
    CHECK(d.removeFirstIf([](const int& x) { return x == 3; }));
    CHECK(!d.removeFirstIf([](const int&) { return false; }));
    CHECK((d.toVector() == std::vector<int>{2}));
}

static void peek_and_clear() {
    Deque<int> d;
    d.pushBack(1);
    d.pushBack(2);
    CHECK(d.peekFront() == 1);
    CHECK(d.peekBack() == 2);
    d.clear();
    CHECK(d.empty());
    d.pushFront(9);
    CHECK((d.toVector() == std::vector<int>{9}));
}

int main() {
    RUN(empty_at_start);
    RUN(push_order);
    RUN(pop_both_ends);
    RUN(pop_last_element_resets);
    RUN(empty_throws);
    RUN(remove_middle);
    RUN(remove_ends_and_missing);
    RUN(peek_and_clear);
    TEST_MAIN_END();
}


