// Kiểm thử sơ đồ ghế. Người phụ trách: B
#include "../src/core/SeatMap.h"
#include "check.h"

static void layout_types() {
    SeatMap m(6, 8, 2);
    CHECK(m.typeAt(0, 0) == SeatType::Normal);
    CHECK(m.typeAt(3, 7) == SeatType::Normal);
    CHECK(m.typeAt(4, 0) == SeatType::Vip);
    CHECK(m.typeAt(5, 7) == SeatType::Vip);
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
}

static void find_free_prefers_center() {
    SeatMap m(5, 7, 1);
    auto p = m.findFree(SeatType::Normal);
    CHECK(p.has_value());
    CHECK(p->col == 3);             // giữa dãy của 7 cột
    CHECK(m.typeAt(p->row, p->col) == SeatType::Normal);
}

static void find_free_when_full() {
    SeatMap m(2, 2, 1);
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 2; ++c) m.occupy(r, c);
    CHECK(!m.findFree(SeatType::Normal).has_value());
    CHECK(!m.findFree(SeatType::Vip).has_value());
    CHECK(m.freeCount(SeatType::Vip) == 0);
}

static void seat_name() {
    CHECK(SeatMap::seatName(0, 0) == "A1");
    CHECK(SeatMap::seatName(2, 11) == "C12");
}

int main() {
    RUN(layout_types);
    RUN(occupy_and_release);
    RUN(find_free_prefers_center);
    RUN(find_free_when_full);
    RUN(seat_name);
    TEST_MAIN_END();
}
