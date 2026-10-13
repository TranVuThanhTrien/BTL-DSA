// Kiểm thử lưu file, thanh toán, xuất vé. Người phụ trách: D
#include <cstdio>

#include "../src/storage/Storage.h"
#include "../src/storage/Ticketing.h"
#include "check.h"

static Ticket sample(int id, const std::string& name) {
    Ticket t;
    t.id = id; t.customerName = name; t.type = SeatType::Vip;
    t.row = 5; t.col = 3; t.price = PRICE_VIP;
    t.status = TicketStatus::Paid; t.createdAt = 1700000000;
    return t;
}

static void save_then_load_roundtrip() {
    const std::string path = "test_tickets.csv";
    saveTickets(path, {sample(1, "An"), sample(2, "Binh, Nguyen")});   // tên có dấu phẩy
    auto back = loadTickets(path);
    std::remove(path.c_str());
    CHECK(back.size() == 2);
    CHECK(back[1].customerName == "Binh, Nguyen");
    CHECK(back[0].status == TicketStatus::Paid);
}

static void load_missing_file_is_empty() {
    CHECK(loadTickets("khong_ton_tai.csv").empty());
}

static void load_bad_line_throws_with_message() {
    const std::string path = "test_bad.csv";
    FILE* f = std::fopen(path.c_str(), "w");
    std::fputs("id,name,type,row,col,price,status,createdAt\nabc,loi\n", f);
    std::fclose(f);
    bool threw = false;
    try { loadTickets(path); } catch (const StorageError&) { threw = true; }
    std::remove(path.c_str());
    CHECK(threw);
}

static void pay_exact_and_with_change() {
    Ticket t = sample(1, "An");
    t.status = TicketStatus::Unpaid;
    auto r = payTicket(t, 200000);
    CHECK(r.ok);
    CHECK(r.change == 200000 - PRICE_VIP);
    CHECK(t.status == TicketStatus::Paid);
}

static void pay_not_enough_or_already_paid() {
    Ticket t = sample(1, "An");
    t.status = TicketStatus::Unpaid;
    CHECK(!payTicket(t, 1000).ok);
    CHECK(t.status == TicketStatus::Unpaid);
    t.status = TicketStatus::Paid;
    CHECK(!payTicket(t, 999999).ok);
}

int main() {
    RUN(save_then_load_roundtrip);
    RUN(load_missing_file_is_empty);
    RUN(load_bad_line_throws_with_message);
    RUN(pay_exact_and_with_change);
    RUN(pay_not_enough_or_already_paid);
    TEST_MAIN_END();
}
