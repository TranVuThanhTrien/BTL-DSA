// Kiểm thử điều phối vé và hàng chờ. Người phụ trách: C
#include "../src/core/BookingService.h"
#include "check.h"

// Phòng nhỏ: 2 hàng x 2 cột, hàng cuối là VIP -> 2 ghế thường, 2 ghế VIP.
static void fill_normal(BookingService& s) {
    s.book("N1", SeatType::Normal);
    s.book("N2", SeatType::Normal);
}

static void book_when_free() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    auto r = s.book("An", SeatType::Normal);
    CHECK(r.seated);
    CHECK(r.ticket.status == TicketStatus::Unpaid);
    CHECK(r.ticket.price == PRICE_NORMAL);
}

static void normal_joins_back_vip_joins_front() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    fill_normal(s);
    s.book("Thuong1", SeatType::Normal);       // hết ghế -> vào hàng
    auto r = s.book("Thuong2", SeatType::Normal);
    CHECK(!r.seated);
    CHECK(r.queuePosition == 2);
    s.book("V1", SeatType::Vip);
    s.book("V2", SeatType::Vip);
    auto v = s.book("VipCho", SeatType::Vip);  // hết ghế VIP -> đầu hàng
    CHECK(!v.seated);
    CHECK(v.queuePosition == 1);
    CHECK(s.queueSnapshot().front().name == "VipCho");
    CHECK(s.queueSnapshot().back().name == "Thuong2");
}

static void cancel_paid_refunds_80_percent() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    auto t = s.book("An", SeatType::Normal).ticket;
    s.findTicket(t.id)->status = TicketStatus::Paid;
    auto c = s.cancel(t.id);
    CHECK(c.refund == PRICE_NORMAL * REFUND_PERCENT / 100);
    CHECK(c.cancelled.status == TicketStatus::Cancelled);
}

static void cancel_unpaid_no_refund() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    auto t = s.book("An", SeatType::Normal).ticket;
    CHECK(s.cancel(t.id).refund == 0);
}

static void cancel_gives_seat_to_queue_head() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    fill_normal(s);
    s.book("Cho1", SeatType::Normal);
    s.book("Cho2", SeatType::Normal);
    auto c = s.cancel(1);
    CHECK(c.granted.size() == 1);
    CHECK(c.granted[0].customerName == "Cho1");
    CHECK(s.queueSnapshot().size() == 1);
}

static void cancel_skips_wrong_seat_type() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    fill_normal(s);
    s.book("V1", SeatType::Vip);
    s.book("V2", SeatType::Vip);
    s.book("VipCho", SeatType::Vip);           // đầu hàng, cần ghế VIP
    s.book("ThuongCho", SeatType::Normal);     // sau đó
    auto c = s.cancel(1);                      // trả ghế THƯỜNG
    CHECK(c.granted.size() == 1);
    CHECK(c.granted[0].customerName == "ThuongCho");   // bỏ qua VipCho, gỡ giữa chuỗi
    CHECK(s.queueSnapshot().size() == 1);
    CHECK(s.queueSnapshot()[0].name == "VipCho");
}

static void cancel_unknown_ticket_throws() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    bool threw = false;
    try { s.cancel(99); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

int main() {
    RUN(book_when_free);
    RUN(normal_joins_back_vip_joins_front);
    RUN(cancel_paid_refunds_80_percent);
    RUN(cancel_unpaid_no_refund);
    RUN(cancel_gives_seat_to_queue_head);
    RUN(cancel_skips_wrong_seat_type);
    RUN(cancel_unknown_ticket_throws);
    TEST_MAIN_END();
}
