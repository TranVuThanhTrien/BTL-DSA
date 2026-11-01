// Kiểm thử phòng vé: giữ ghế, thanh toán, hủy hoàn tiền, hàng chờ, khuyến mãi, soát vé, lưu và nạp.
#include <cstdlib>
#include <filesystem>

#include "../src/box/BoxOffice.h"
#include "check.h"

namespace fs = std::filesystem;
static const std::time_t NOW = 1800000000;  // một thời điểm cố định
static int g_dirCount = 0;

struct Fixture {
    std::string dir;
    BoxOffice box;
    int show = 0;  // suất 2x2 ghế (2 thường + 2 VIP), chiếu sau 2 ngày
    static std::string makeDir() {
        const std::string d = (fs::temp_directory_path() / ("cq_box_test_" + std::to_string(g_dirCount++) + "_" + std::to_string(std::rand()))).string();
        fs::remove_all(d);
        fs::create_directories(d);
        return d;
    }
    explicit Fixture(const std::string& d = makeDir()) : dir(d), box(d) {
        box.load(NOW);
        box.resetSales();  // bỏ đơn mẫu để mã đơn bắt đầu từ 1
        Showtime s;
        s.movieId = 1; s.room = "Phong Thu"; s.start = NOW + 2 * 86400; s.rows = 2; s.cols = 2; s.vipRows = 1;
        s.priceNormal = 100000; s.priceVip = 150000;
        show = box.saveShowtime(s);
    }
    ~Fixture() { fs::remove_all(dir); }
    OrderRequest req(const std::string& name, int count, SeatType t = SeatType::Normal, const std::string& owner = "") {
        OrderRequest r; r.showtimeId = show; r.name = name; r.owner = owner; r.autoCount = count; r.autoType = t;
        return r;
    }
};

template <typename F>
static bool throwsBox(F fn, int status = 0) {
    try { fn(); } catch (const BoxError& e) { return status == 0 || e.status == status; }
    return false;
}

static void order_holds_seats_and_totals() {
    Fixture f;
    auto o = f.box.createOrder(f.req("An", 2), NOW);
    CHECK(o.status == OrderStatus::Hold);
    CHECK(o.ticketIds.size() == 2);
    CHECK(o.ticketsTotal == 200000 && o.total == 200000);
    CHECK(o.holdUntil == NOW + HOLD_SECONDS);
    CHECK(f.box.seatMap(f.show).freeCount(SeatType::Normal) == 0);
    CHECK(throwsBox([&] { f.box.createOrder(f.req("B", 1), NOW); }));  // hết ghế thường
    CHECK(BoxOffice::orderCode(o.id) == "CQ000001");
}

static void hold_expires_and_frees_seats() {
    Fixture f;
    auto o = f.box.createOrder(f.req("An", 1), NOW);
    CHECK(f.box.sweep(NOW + 100) == 0);
    CHECK(f.box.sweep(NOW + HOLD_SECONDS + 1) == 1);
    CHECK(f.box.findOrder(o.id)->status == OrderStatus::Expired);
    CHECK(f.box.seatMap(f.show).freeCount(SeatType::Normal) == 2);
    CHECK(throwsBox([&] { f.box.pay(o.id, "BANK", 0, false, NOW + HOLD_SECONDS + 5); }));
}

static void pay_methods() {
    Fixture f;
    auto o = f.box.createOrder(f.req("An", 1, SeatType::Normal, "an"), NOW);
    CHECK(throwsBox([&] { f.box.pay(o.id, "CASH", 500000, false, NOW); }, 403));   // khách online không trả tiền mặt
    CHECK(throwsBox([&] { f.box.pay(o.id, "CASH", 50000, true, NOW); }));          // thiếu tiền
    CHECK(throwsBox([&] { f.box.pay(o.id, "BITCOIN", 0, true, NOW); }));
    auto p = f.box.pay(o.id, "WALLET", 0, false, NOW + 30);
    CHECK(p.order.status == OrderStatus::Paid);
    CHECK(p.order.payRef.rfind("TX", 0) == 0);
    CHECK(f.box.ticketsOf(p.order)[0].status == TicketStatus::Paid);
    CHECK(throwsBox([&] { f.box.pay(o.id, "WALLET", 0, false, NOW + 40); }));      // trả hai lần
    auto o2 = f.box.createOrder(f.req("Binh", 1), NOW);
    auto p2 = f.box.pay(o2.id, "CASH", 120000, true, NOW);
    CHECK(p2.change == 20000);
}

static void cancel_refund_rules() {
    Fixture f;
    auto o = f.box.createOrder(f.req("An", 1, SeatType::Normal, "an"), NOW);
    auto c0 = f.box.cancel(o.id, false, NOW + 10);       // chưa trả tiền: hủy tự do, không hoàn
    CHECK(c0.refund == 0 && c0.order.status == OrderStatus::Cancelled);
    CHECK(f.box.seatMap(f.show).freeCount(SeatType::Normal) == 2);
    CHECK(throwsBox([&] { f.box.cancel(o.id, false, NOW + 20); }));

    auto o2 = f.box.createOrder(f.req("Binh", 1, SeatType::Normal, "binh"), NOW);
    f.box.pay(o2.id, "BANK", 0, false, NOW);
    auto c = f.box.cancel(o2.id, false, NOW + 100);       // còn hơn 2 tiếng: hoàn 80%
    CHECK(c.refund == 100000 * REFUND_PERCENT / 100);

    auto o3 = f.box.createOrder(f.req("Chi", 1, SeatType::Normal, "chi"), NOW);
    f.box.pay(o3.id, "BANK", 0, false, NOW);
    const std::time_t late = NOW + 2 * 86400 - 3600;      // còn 1 tiếng
    CHECK(throwsBox([&] { f.box.cancel(o3.id, false, late); }));
    CHECK(f.box.cancel(o3.id, true, late).refund == 80000);  // quầy vẫn xử lý được
}

static void cancelling_grants_seat_to_queue_head() {
    Fixture f;
    auto o = f.box.createOrder(f.req("An", 2, SeatType::Normal, "an"), NOW);
    auto j1 = f.box.joinQueue(f.show, SeatType::Normal, "Cho1", "u1", NOW);
    auto j2 = f.box.joinQueue(f.show, SeatType::Normal, "Cho2", "u2", NOW);
    CHECK(j1.position == 1 && j2.position == 2);
    CHECK(throwsBox([&] { f.box.joinQueue(f.show, SeatType::Normal, "Cho1b", "u1", NOW); }));  // một chỗ chờ mỗi người
    auto c = f.box.cancel(o.id, false, NOW + 5);
    CHECK(c.granted.size() == 2);
    CHECK(c.granted[0].name == "Cho1" && c.granted[1].name == "Cho2");
    CHECK(c.granted[0].status == OrderStatus::Hold && c.granted[0].fromQueue);
    CHECK(f.box.queue(f.show).empty());
    // người được cấp ghế không thanh toán kịp thì ghế quay lại hàng chờ tiếp theo
    f.box.joinQueue(f.show, SeatType::Normal, "Cho3", "u3", NOW);
    CHECK(f.box.sweep(NOW + HOLD_SECONDS + 10) >= 2);
    CHECK(f.box.findOrder(c.granted[0].id)->status == OrderStatus::Expired);
}

static void vip_jumps_queue_and_skips_wrong_type() {
    Fixture f;
    auto a = f.box.createOrder(f.req("An", 2, SeatType::Normal), NOW);
    auto b = f.box.createOrder(f.req("Binh", 2, SeatType::Vip), NOW);
    f.box.joinQueue(f.show, SeatType::Normal, "T1", "", NOW);
    f.box.joinQueue(f.show, SeatType::Vip, "V1", "", NOW);
    auto q = f.box.queue(f.show);
    CHECK(q.size() == 2 && q[0].name == "V1" && q[1].name == "T1");      // VIP lên đầu
    auto c = f.box.cancel(a.id, true, NOW);                               // trả 2 ghế thường: VIP bị bỏ qua
    CHECK(c.granted.size() == 1 && c.granted[0].name == "T1");
    CHECK(f.box.queue(f.show).size() == 1 && f.box.queue(f.show)[0].name == "V1");
    (void)b;
    f.box.leaveQueue(f.show, f.box.queue(f.show)[0].id, "", true);
    CHECK(f.box.queue(f.show).empty());
    CHECK(throwsBox([&] { f.box.leaveQueue(f.show, 999, "", true); }, 404));
}

static void promo_codes() {
    Fixture f;
    auto r = f.req("An", 2);
    r.promo = "cine10";
    auto q = f.box.quote(r, NOW);
    CHECK(q.ticketsTotal == 200000 && q.discount == 20000 && q.total == 180000);
    r.promo = "VUIVE20";   // cần đơn vé từ 300.000 đ
    CHECK(throwsBox([&] { f.box.quote(r, NOW); }));
    r.promo = "SAIMA";
    CHECK(throwsBox([&] { f.box.quote(r, NOW); }));
    r.promo = "";
    r.combos["C1"] = 2;
    q = f.box.quote(r, NOW);
    CHECK(q.comboTotal == 138000 && q.total == 338000);
    r.combos["KHONGCO"] = 1;
    CHECK(throwsBox([&] { f.box.quote(r, NOW); }));
}

static void order_limits() {
    Fixture f;
    auto r = f.req("An", MAX_SEATS_PER_ORDER + 1);
    CHECK(throwsBox([&] { f.box.createOrder(r, NOW); }));
    CHECK(throwsBox([&] { f.box.createOrder(f.req("", 1), NOW); }));
    f.box.createOrder(f.req("An", 1, SeatType::Normal, "an"), NOW);
    f.box.createOrder(f.req("An", 1, SeatType::Vip, "an"), NOW);
    CHECK(throwsBox([&] { f.box.createOrder(f.req("An", 1, SeatType::Vip, "an"), NOW); }));  // quá số đơn giữ chỗ
    OrderRequest manual = f.req("X", 0);
    manual.seats = {{0, 0}, {0, 0}};
    CHECK(throwsBox([&] { f.box.createOrder(manual, NOW); }));   // ghế trùng / đã có người
    // ngừng bán trước giờ chiếu
    CHECK(throwsBox([&] { f.box.createOrder(f.req("Z", 1), NOW + 2 * 86400 - 5 * 60); }));
}

static void checkin_rules() {
    Fixture f;
    auto o = f.box.createOrder(f.req("An", 1), NOW);
    const std::time_t open = NOW + 2 * 86400 - 30 * 60;
    CHECK(throwsBox([&] { f.box.checkIn(BoxOffice::orderCode(o.id), open); }));            // chưa thanh toán
    f.box.pay(o.id, "BANK", 0, false, NOW);
    CHECK(throwsBox([&] { f.box.checkIn(BoxOffice::orderCode(o.id), NOW + 100); }));       // quá sớm
    CHECK(throwsBox([&] { f.box.checkIn("ABC", open); }));
    CHECK(throwsBox([&] { f.box.checkIn("CQ999999", open); }, 404));
    auto p = f.box.checkIn(" cq000001 ", open);
    CHECK(p.order.checkedInAt == open);
    CHECK(throwsBox([&] { f.box.checkIn("CQ000001", open + 5); }));                         // vé đã dùng
    CHECK(throwsBox([&] { f.box.cancel(o.id, true, open + 10); }));                         // đã dùng thì không hủy
    CHECK(BoxOffice::parseOrderCode("CQ12-A1") == 12);
    CHECK(BoxOffice::parseOrderCode("12") == 0);
}

static void stats_add_up() {
    Fixture f;
    auto a = f.box.createOrder(f.req("An", 1), NOW);
    f.box.pay(a.id, "BANK", 0, false, NOW);
    auto b = f.box.createOrder(f.req("Binh", 1), NOW);
    f.box.pay(b.id, "BANK", 0, false, NOW);
    f.box.cancel(b.id, true, NOW);
    auto s = f.box.stats();
    CHECK(s.gross >= 200000 && s.refunds == 80000);
    CHECK(s.net == s.gross - s.refunds);
    CHECK(s.ordersCancelled == 1);
}

static void save_and_reload_round_trip() {
    Fixture f;
    auto a = f.box.createOrder(f.req("Nguyễn, \"An\"", 2, SeatType::Normal, "an"), NOW);
    f.box.pay(a.id, "BANK", 0, false, NOW);
    auto b = f.box.createOrder(f.req("Binh", 1, SeatType::Vip, "binh"), NOW);
    f.box.joinQueue(f.show, SeatType::Normal, "Cho", "cho", NOW);
    f.box.save();
    BoxOffice again(f.dir);
    again.load(NOW);
    CHECK(again.movies().size() == f.box.movies().size());
    CHECK(again.showtimes().size() == f.box.showtimes().size());
    CHECK(again.orders().size() == 2);
    const Order* o = again.findOrder(a.id);
    CHECK(o && o->name == "Nguyễn, \"An\"" && o->status == OrderStatus::Paid && o->total == 200000);
    CHECK(again.ticketsOf(*o).size() == 2);
    CHECK(again.seatMap(f.show).freeCount(SeatType::Normal) == 0);
    CHECK(again.seatMap(f.show).freeCount(SeatType::Vip) == 1);
    CHECK(again.queue(f.show).size() == 1 && again.queue(f.show)[0].name == "Cho");
    auto c = again.createOrder(f.req("Moi", 1, SeatType::Vip), NOW);   // mã đơn tiếp tục tăng sau khi nạp lại
    CHECK(c.id == b.id + 1);
}

static void catalog_validation() {
    Fixture f;
    Movie m; m.title = "  "; 
    CHECK(throwsBox([&] { f.box.saveMovie(m); }));
    m.title = "Phim Moi"; m.minutes = 10;
    CHECK(throwsBox([&] { f.box.saveMovie(m); }));
    m.minutes = 100;
    int id = f.box.saveMovie(m);
    Showtime s; s.movieId = id; s.room = "Phong Thu"; s.start = NOW + 2 * 86400 + 600; s.rows = 2; s.cols = 2; s.vipRows = 1;
    s.priceNormal = 1; s.priceVip = 2;
    CHECK(throwsBox([&] { f.box.saveShowtime(s); }));                 // trùng giờ cùng phòng
    s.room = "Phong Khac";
    int sid = f.box.saveShowtime(s);
    CHECK(throwsBox([&] { f.box.removeMovie(id); }));                 // phim còn suất chiếu
    f.box.removeShowtime(sid);
    f.box.removeMovie(id);
    auto o = f.box.createOrder(f.req("An", 1), NOW);
    CHECK(throwsBox([&] { f.box.removeShowtime(f.show); }));          // suất đã có đơn
    (void)o;
}

int main() {
    std::cout << "BoxOffice\n";
    RUN(order_holds_seats_and_totals);
    RUN(hold_expires_and_frees_seats);
    RUN(pay_methods);
    RUN(cancel_refund_rules);
    RUN(cancelling_grants_seat_to_queue_head);
    RUN(vip_jumps_queue_and_skips_wrong_type);
    RUN(promo_codes);
    RUN(order_limits);
    RUN(checkin_rules);
    RUN(stats_add_up);
    RUN(save_and_reload_round_trip);
    RUN(catalog_validation);
    TEST_MAIN_END();
}
