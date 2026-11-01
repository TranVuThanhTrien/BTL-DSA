// Kiểm thử lưu file, thanh toán, xuất vé.
#include <cstdio>
#include <fstream>

#include "../src/storage/Storage.h"
#include "../src/storage/Ticketing.h"
#include "check.h"

static Ticket sample(int id, const std::string& name) {
    Ticket t;
    t.id = id; t.customerName = name; t.type = SeatType::Vip;
    t.row = 4; t.col = 3; t.price = PRICE_VIP;
    t.status = TicketStatus::Paid; t.createdAt = 1700000000; t.owner = "khach"; t.refund = 0;
    return t;
}

static void tickets_roundtrip() {
    const std::string path = "test_tickets.csv";
    auto b = sample(2, "Binh, \"Bo\" Nguyen");        // dấu phẩy và nháy kép trong tên
    b.status = TicketStatus::Cancelled; b.refund = 96000;
    saveTickets(path, {sample(1, "An"), b});
    auto back = loadTickets(path);
    std::remove(path.c_str());
    CHECK(back.size() == 2);
    CHECK(back[1].customerName == "Binh, \"Bo\" Nguyen");
    CHECK(back[0].status == TicketStatus::Paid);
    CHECK(back[0].owner == "khach");
    CHECK(back[1].refund == 96000);
    CHECK(back[1].status == TicketStatus::Cancelled);
}

static void queue_roundtrip() {
    const std::string path = "test_queue.csv";
    Customer a; a.id = 1; a.name = "An"; a.type = SeatType::Vip; a.owner = "an";
    Customer b; b.id = 2; b.name = "Bình"; b.type = SeatType::Normal;
    saveQueue(path, {a, b});
    auto back = loadQueue(path);
    std::remove(path.c_str());
    CHECK(back.size() == 2);
    CHECK(back[0].name == "An" && back[0].type == SeatType::Vip && back[0].owner == "an");
    CHECK(back[1].name == "Bình");
}

static void missing_file_is_empty() {
    CHECK(loadTickets("khong_ton_tai.csv").empty());
    CHECK(loadQueue("khong_ton_tai.csv").empty());
}

static void bad_line_throws_with_line_number() {
    const std::string path = "test_bad.csv";
    std::ofstream(path) << "id,name,type,row,col,price,status,createdAt\nabc,loi\n";
    std::string msg;
    try { loadTickets(path); } catch (const StorageError& e) { msg = e.what(); }
    std::remove(path.c_str());
    CHECK(msg.find("Dong 2") != std::string::npos);
}

static void bad_number_and_bad_enum_throw() {
    const std::string path = "test_bad2.csv";
    std::ofstream(path) << "h\n1,An,VIP,x,3,120000,PAID,1700000000\n";
    bool a = false, b = false;
    try { loadTickets(path); } catch (const StorageError&) { a = true; }
    std::ofstream(path) << "h\n1,An,VIP,4,3,120000,LA,1700000000\n";
    try { loadTickets(path); } catch (const StorageError&) { b = true; }
    std::remove(path.c_str());
    CHECK(a && b);
}

static void old_format_without_owner_loads() {
    const std::string path = "test_old.csv";
    std::ofstream(path) << "h\n1,An,NORMAL,0,0,80000,PAID,1700000000\n";
    auto v = loadTickets(path);
    std::remove(path.c_str());
    CHECK(v.size() == 1 && v[0].owner.empty());
}

static void csv_helpers() {
    CHECK(csvQuote("abc") == "abc");
    CHECK(csvQuote("a,b") == "\"a,b\"");
    auto f = csvSplit("1,\"a,\"\"b\"\"\",,x");
    CHECK(f.size() == 4 && f[1] == "a,\"b\"" && f[2].empty());
}

static void pay_exact_and_with_change() {
    Ticket t = sample(1, "An");
    t.status = TicketStatus::Unpaid;
    auto r = payTicket(t, 200000);
    CHECK(r.ok);
    CHECK(r.change == 200000 - PRICE_VIP);
    CHECK(t.status == TicketStatus::Paid);
}

static void pay_not_enough_paid_or_cancelled() {
    Ticket t = sample(1, "An");
    t.status = TicketStatus::Unpaid;
    auto r = payTicket(t, 1000);
    CHECK(!r.ok && !r.message.empty());
    CHECK(t.status == TicketStatus::Unpaid);
    t.status = TicketStatus::Paid;
    CHECK(!payTicket(t, 999999).ok);
    t.status = TicketStatus::Cancelled;
    CHECK(!payTicket(t, 999999).ok);
}

static void money_and_code_format() {
    CHECK(formatMoney(80000) == "80.000 đ");
    CHECK(formatMoney(1200000) == "1.200.000 đ");
    CHECK(formatMoney(0) == "0 đ");
    CHECK(ticketCode(12) == "VE0012");
}

static void ticket_file_written() {
    const std::string path = "test_ticket.txt";
    writeTicketFile(path, sample(7, "An"), "Phim X", "18:00", "Phòng 1");
    auto lines = readLines(path);
    std::remove(path.c_str());
    CHECK(!lines.empty());
    bool hasCode = false;
    for (auto& l : lines) if (l.find("VE0007") != std::string::npos) hasCode = true;
    CHECK(hasCode);
}

static void unwritable_path_throws() {
    bool threw = false;
    try { saveQueue("/proc/khong/the/ghi.csv", {}); } catch (const StorageError&) { threw = true; }
    CHECK(threw);
}

int main() {
    RUN(tickets_roundtrip);
    RUN(queue_roundtrip);
    RUN(missing_file_is_empty);
    RUN(bad_line_throws_with_line_number);
    RUN(bad_number_and_bad_enum_throw);
    RUN(old_format_without_owner_loads);
    RUN(csv_helpers);
    RUN(pay_exact_and_with_change);
    RUN(pay_not_enough_paid_or_cancelled);
    RUN(money_and_code_format);
    RUN(ticket_file_written);
    RUN(unwritable_path_throws);
    TEST_MAIN_END();
}


