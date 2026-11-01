// Kiểm thử điều phối vé và hàng chờ.
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
    auto r = s.book("An", SeatType::Normal, "an");
    CHECK(r.seated);
    CHECK(r.ticket.status == TicketStatus::Unpaid);
    CHECK(r.ticket.price == PRICE_NORMAL);
    CHECK(r.ticket.owner == "an");
}

static void preferred_seat_is_respected_or_ignored() {
    SeatMap m(3, 3, 1);
    BookingService s(m);
    auto a = s.book("A", SeatType::Normal, "", Position{0, 0});
    CHECK(a.ticket.row == 0 && a.ticket.col == 0);
    auto b = s.book("B", SeatType::Normal, "", Position{0, 0});   // đã có người
    CHECK(b.seated && !(b.ticket.row == 0 && b.ticket.col == 0));
    auto c = s.book("C", SeatType::Normal, "", Position{2, 1});   // ghế VIP nhưng đặt vé thường
    CHECK(c.seated && c.ticket.row != 2);
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
    CHECK(s.findTicket(t.id)->refund == c.refund);
    CHECK(m.isFree(t.row, t.col));
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
    s.book("Cho1", SeatType::Normal, "u1");
    s.book("Cho2", SeatType::Normal);
    auto c = s.cancel(1);
    CHECK(c.granted.size() == 1);
    CHECK(c.granted[0].customerName == "Cho1");
    CHECK(c.granted[0].owner == "u1");                 // giữ nguyên người đặt
    CHECK(c.granted[0].status == TicketStatus::Unpaid);
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

static void cancel_unknown_or_twice_throws() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    bool a = false, b = false;
    try { s.cancel(99); } catch (const std::invalid_argument&) { a = true; }
    auto t = s.book("An", SeatType::Normal).ticket;
    s.cancel(t.id);
    try { s.cancel(t.id); } catch (const std::invalid_argument&) { b = true; }
    CHECK(a && b);
}

static void leave_queue_removes_middle() {
    SeatMap m(1, 1, 0);
    BookingService s(m);
    s.book("Giu", SeatType::Normal);
    s.book("A", SeatType::Normal);
    s.book("B", SeatType::Normal);
    s.book("C", SeatType::Normal);
    auto q = s.queueSnapshot();
    Customer removed;
    CHECK(s.leaveQueue(q[1].id, &removed));
    CHECK(removed.name == "B");
    CHECK(s.queueSnapshot().size() == 2);
    CHECK(!s.leaveQueue(9999));
}

static void restore_rebuilds_seats_and_ids() {
    SeatMap m1(2, 2, 1);
    BookingService a(m1);
    fill_normal(a);
    a.book("Cho", SeatType::Normal);
    SeatMap m2(2, 2, 1);
    BookingService b(m2);
    b.restore(a.tickets(), a.queueSnapshot());
    CHECK(!m2.isFree(0, 1) || !m2.isFree(0, 0));
    CHECK(m2.freeCount(SeatType::Normal) == 0);
    CHECK(b.queueSnapshot().size() == 1);
    auto t = b.book("Moi", SeatType::Vip).ticket;          // id vé mới không trùng id cũ
    CHECK(t.id == 3);
}

static void restore_rejects_duplicate_seat() {
    SeatMap m(2, 2, 1);
    BookingService s(m);
    Ticket x; x.id = 1; x.row = 0; x.col = 0; x.status = TicketStatus::Paid;
    Ticket y = x; y.id = 2;
    bool threw = false;
    try { s.restore({x, y}, {}); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

int main() {
    RUN(book_when_free);
    RUN(preferred_seat_is_respected_or_ignored);
    RUN(normal_joins_back_vip_joins_front);
    RUN(cancel_paid_refunds_80_percent);
    RUN(cancel_unpaid_no_refund);
    RUN(cancel_gives_seat_to_queue_head);
    RUN(cancel_skips_wrong_seat_type);
    RUN(cancel_unknown_or_twice_throws);
    RUN(leave_queue_removes_middle);
    RUN(restore_rebuilds_seats_and_ids);
    RUN(restore_rejects_duplicate_seat);
    TEST_MAIN_END();
}
