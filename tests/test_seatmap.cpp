// Kiểm thử sơ đồ ghế.
#include "../src/core/SeatMap.h"
#include "check.h"

static void layout_types() {
    SeatMap m(6, 8, 2);             // VIP ở hàng D, E; hàng F là thường
    CHECK(m.typeAt(0, 0) == SeatType::Normal);
    CHECK(m.typeAt(2, 7) == SeatType::Normal);
    CHECK(m.typeAt(3, 0) == SeatType::Vip);
    CHECK(m.typeAt(4, 7) == SeatType::Vip);
    CHECK(m.typeAt(5, 7) == SeatType::Normal);
    CHECK(m.freeCount(SeatType::Normal) == 32);
    CHECK(m.freeCount(SeatType::Vip) == 16);
}

static void occupy_and_release() {
    SeatMap m(4, 4, 1);
    CHECK(m.occupy(0, 0));
    CHECK(!m.occupy(0, 0));         // đã có người
    CHECK(!m.isFree(0, 0));
    m.release(0, 0);
    CHECK(m.isFree(0, 0));
    CHECK(!m.occupy(9, 9));         // ngoài phạm vi
    CHECK(!m.occupy(-1, 0));
}

static void find_free_prefers_center() {
    SeatMap m(5, 7, 1);             // VIP ở hàng D (chỉ số 3)
    auto p = m.findFree(SeatType::Normal);
    CHECK(p.has_value());
    CHECK(p->col == 3);             // giữa dãy của 7 cột
    CHECK(m.typeAt(p->row, p->col) == SeatType::Normal);
    auto v = m.findFree(SeatType::Vip);
    CHECK(v.has_value() && v->row == 3);
}

static void find_free_skips_taken() {
    SeatMap m(5, 7, 1);
    auto first = *m.findFree(SeatType::Normal);
    m.occupy(first.row, first.col);
    auto second = *m.findFree(SeatType::Normal);
    CHECK(!(second.row == first.row && second.col == first.col));
}

static void find_free_when_full() {
    SeatMap m(4, 2, 1);             // hàng A-C thường (6 ghế), hàng D VIP (2 ghế)
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 2; ++c) m.occupy(r, c);
    CHECK(!m.findFree(SeatType::Normal).has_value());
    CHECK(!m.findFree(SeatType::Vip).has_value());
    CHECK(m.freeCount(SeatType::Vip) == 0);
    m.clear();
    CHECK(m.freeCount(SeatType::Normal) == 6);
}

static void room_without_vip() {
    SeatMap m(3, 3, 0);
    CHECK(!m.findFree(SeatType::Vip).has_value());
    CHECK(m.freeCount(SeatType::Vip) == 0);
}

static void invalid_size_throws() {
    bool threw = false;
    try { SeatMap m(0, 5, 0); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

static void seat_name() {
    CHECK(SeatMap::seatName(0, 0) == "A1");
    CHECK(SeatMap::seatName(2, 11) == "C12");
}

int main() {
    RUN(layout_types);
    RUN(occupy_and_release);
    RUN(find_free_prefers_center);
    RUN(find_free_skips_taken);
    RUN(find_free_when_full);
    RUN(room_without_vip);
    RUN(invalid_size_throws);
    RUN(seat_name);
    TEST_MAIN_END();
}
