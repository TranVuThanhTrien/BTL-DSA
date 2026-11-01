// Bản console đầy đủ: đăng nhập khách / quản trị, sơ đồ ghế có màu, chọn ghế, thanh toán, hủy, hàng chờ (Deque),
// soát vé, thống kê. Dùng chung dữ liệu (thư mục data/) và cùng phần lõi với server web,
// nhưng KHÔNG chạy console và server cùng lúc vì hai chương trình sẽ ghi đè dữ liệu của nhau.
//
// Quy ước: mọi thao tác ghi đều gọi box.save() ngay sau khi thành công. Thời gian hiện tại truyền vào `now`.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <cstdio>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "../box/BoxOffice.h"
#include "../server/Auth.h"
#include "../server/News.h"
#include "../storage/Storage.h"
#include "../storage/Ticketing.h"

// ======================= màu và tiện ích hiển thị =======================
namespace col {
const char* reset = "\033[0m";
const char* bold = "\033[1m";
const char* dim = "\033[2m";
const char* red = "\033[31m";
const char* green = "\033[32m";
const char* yellow = "\033[33m";
const char* blue = "\033[34m";
const char* magenta = "\033[35m";
const char* cyan = "\033[36m";
const char* gray = "\033[90m";
}  // namespace col

static bool g_color = true;
static std::string C(const char* code, const std::string& s) { return g_color ? std::string(code) + s + col::reset : s; }

static void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode) || !SetConsoleMode(h, mode | 0x0004))
        g_color = false;  // terminal cũ không hỗ trợ màu ANSI
#endif
    if (const char* nc = std::getenv("NO_COLOR")) if (*nc) g_color = false;
}

// Số ô hiển thị của chuỗi UTF-8 (mỗi ký tự một ô, đủ dùng cho tiếng Việt).
static int cells(const std::string& s) {
    int n = 0;
    for (unsigned char ch : s) if ((ch & 0xC0) != 0x80) ++n;
    return n;
}
static std::string pad(const std::string& s, int w) { return s + std::string(std::max(0, w - cells(s)), ' '); }
static std::string padLeft(const std::string& s, int w) { return std::string(std::max(0, w - cells(s)), ' ') + s; }
static std::string rep(const std::string& s, int n) { std::string r; for (int i = 0; i < n; ++i) r += s; return r; }

static std::string vnd(long v) { return formatMoney(v); }
static std::string hhmm(std::time_t t) { char b[16]; std::tm m = *std::localtime(&t); std::strftime(b, sizeof b, "%H:%M", &m); return b; }
static std::string dateStr(std::time_t t) { char b[16]; std::tm m = *std::localtime(&t); std::strftime(b, sizeof b, "%d/%m", &m); return b; }
static std::string dateTime(std::time_t t) { return hhmm(t) + " " + dateStr(t); }

static void title(const std::string& s) {
    std::cout << "\n" << C(col::cyan, C(col::bold, "━━ " + s + " " + rep("━", std::max(4, 60 - cells(s))))) << "\n";
}
static void ok(const std::string& s) { std::cout << "  " << C(col::green, "✔ " + s) << "\n"; }
static void warn(const std::string& s) { std::cout << "  " << C(col::yellow, "! " + s) << "\n"; }
static void fail(const std::string& s) { std::cout << "  " << C(col::red, "✖ " + s) << "\n"; }
static void note(const std::string& s) { std::cout << "  " << C(col::gray, s) << "\n"; }

// ======================= nhập liệu =======================
static bool g_eof = false;

static std::string ask(const std::string& prompt) {
    std::cout << "  " << C(col::bold, prompt) << " ";
    std::string s;
    if (!std::getline(std::cin, s)) { g_eof = true; return ""; }
    if (!s.empty() && s.back() == '\r') s.pop_back();
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

static long askNumber(const std::string& prompt, long lo, long hi, long def = -1) {
    while (!g_eof) {
        const std::string s = ask(prompt);
        if (g_eof) break;
        if (s.empty() && def >= 0) return def;
        try {
            std::size_t used = 0;
            const long v = std::stol(s, &used);
            if (used == s.size() && v >= lo && v <= hi) return v;
        } catch (const std::exception&) {}
        warn("Vui lòng nhập số từ " + std::to_string(lo) + " đến " + std::to_string(hi) + ".");
    }
    return lo;
}

static bool askYes(const std::string& prompt, bool def = true) {
    const std::string s = ask(prompt + (def ? " (Y/n):" : " (y/N):"));
    if (s.empty()) return def;
    return s[0] == 'y' || s[0] == 'Y' || s[0] == 'c' || s[0] == 'C';  // y / c (có)
}

static std::string askPassword(const std::string& prompt) {
    std::cout << "  " << C(col::bold, prompt) << " " << std::flush;
    std::string s;
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool tty = GetConsoleMode(h, &mode) != 0;
    if (tty) SetConsoleMode(h, mode & ~ENABLE_ECHO_INPUT);
    if (!std::getline(std::cin, s)) g_eof = true;
    if (tty) { SetConsoleMode(h, mode); std::cout << "\n"; }
#else
    termios old{};
    const bool tty = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old) == 0;
    if (tty) { termios t = old; t.c_lflag &= ~static_cast<tcflag_t>(ECHO); tcsetattr(STDIN_FILENO, TCSANOW, &t); }
    if (!std::getline(std::cin, s)) g_eof = true;
    if (tty) { tcsetattr(STDIN_FILENO, TCSANOW, &old); std::cout << "\n"; }
#endif
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

// ======================= trạng thái phiên =======================
struct Ctx {
    BoxOffice& box;
    AuthService& auth;
    NewsService& news;
    std::optional<User> user;
    int showId = 0;  // suất đang chọn
    bool loggedIn() const { return user.has_value(); }
    bool admin() const { return user && user->isAdmin(); }
};

static std::string ownerOf(const Ctx& c) { return c.admin() ? "" : c.user->username; }

// ======================= sơ đồ ghế =======================
enum class Cell { Free, Held, Paid, Mine, Picked };

// Dựng bảng trạng thái ghế của một suất: trống / đang giữ / đã bán / của tôi.
static std::vector<std::vector<Cell>> seatStates(const Ctx& c, int showId) {
    const SeatMap& sm = c.box.seatMap(showId);
    std::vector<std::vector<Cell>> st(sm.rows(), std::vector<Cell>(sm.cols(), Cell::Free));
    for (int r = 0; r < sm.rows(); ++r)
        for (int k = 0; k < sm.cols(); ++k)
            if (!sm.isFree(r, k)) st[r][k] = Cell::Paid;  // bị chiếm nhưng chưa rõ đơn nào
    for (const auto& o : c.box.orders()) {
        if (o.showtimeId != showId) continue;
        if (o.status != OrderStatus::Hold && o.status != OrderStatus::Paid) continue;
        const bool mine = c.user && !o.owner.empty() && o.owner == c.user->username;
        for (const auto& t : c.box.ticketsOf(o)) {
            if (t.status == TicketStatus::Cancelled || !sm.inRange(t.row, t.col)) continue;
            st[t.row][t.col] = mine ? Cell::Mine : (o.status == OrderStatus::Hold ? Cell::Held : Cell::Paid);
        }
    }
    return st;
}

static void drawSeats(const Ctx& c, int showId, const std::vector<Position>& picked = {}) {
    const Showtime* s = c.box.findShowtime(showId);
    if (!s) return;
    const SeatMap& sm = c.box.seatMap(showId);
    auto st = seatStates(c, showId);
    for (const auto& p : picked) if (sm.inRange(p.row, p.col)) st[p.row][p.col] = Cell::Picked;

    const int cols = sm.cols();
    const int gapAt = cols >= 8 ? cols / 2 : -1;       // lối đi giữa phòng
    const int width = 4 + cols * 4 + (gapAt > 0 ? 2 : 0);
    std::cout << "\n";
    const std::string screen = "MÀN HÌNH";
    const int sw = std::max(width - 8, cells(screen) + 6);
    const int left = (width - sw) / 2 + 2;
    std::cout << std::string(std::max(0, left), ' ') << C(col::gray, "╭" + rep("─", sw) + "╮") << "\n";
    std::cout << std::string(std::max(0, left), ' ') << C(col::gray, "│") << C(col::bold, pad(std::string((sw - cells(screen)) / 2, ' ') + screen, sw)) << C(col::gray, "│") << "\n";
    std::cout << std::string(std::max(0, left), ' ') << C(col::gray, "╰" + rep("─", sw) + "╯") << "\n\n";

    std::cout << "      ";
    for (int k = 0; k < cols; ++k) {
        if (k == gapAt) std::cout << "  ";
        std::cout << " " << padLeft(std::to_string(k + 1), 2) << " ";
    }
    std::cout << "\n";
    for (int r = 0; r < sm.rows(); ++r) {
        const bool vipRow = sm.typeAt(r, 0) == SeatType::Vip;
        std::cout << "   " << C(vipRow ? col::yellow : col::gray, std::string(1, static_cast<char>('A' + r))) << "  ";
        for (int k = 0; k < cols; ++k) {
            if (k == gapAt) std::cout << "  ";
            const bool vip = sm.typeAt(r, k) == SeatType::Vip;
            const std::string num = padLeft(std::to_string(k + 1), 2);
            switch (st[r][k]) {
                case Cell::Free:   std::cout << C(vip ? col::yellow : col::green, "[" + num + "]"); break;
                case Cell::Held:   std::cout << C(col::magenta, "[~~]"); break;
                case Cell::Paid:   std::cout << C(col::red, "[XX]"); break;
                case Cell::Mine:   std::cout << C(col::cyan, "[**]"); break;
                case Cell::Picked: std::cout << C(col::bold, "[##]"); break;
            }
        }
        std::cout << "  " << (vipRow ? C(col::yellow, "VIP") : "") << "\n";
    }
    std::cout << "\n  " << C(col::green, "[01]") << " ghế thường trống   " << C(col::yellow, "[01]") << " ghế VIP trống   "
              << C(col::magenta, "[~~]") << " đang giữ   " << C(col::red, "[XX]") << " đã bán\n  "
              << C(col::cyan, "[**]") << " ghế của bạn   " << C(col::bold, "[##]") << " ghế đang chọn\n";
    std::cout << "  Còn trống: " << sm.freeCount(SeatType::Normal) << " ghế thường (" << vnd(s->priceNormal) << "), "
              << sm.freeCount(SeatType::Vip) << " ghế VIP (" << vnd(s->priceVip) << ")\n";
}

// "D3 D4, e5" -> danh sách vị trí; ném BoxError nếu sai cú pháp.
static std::vector<Position> parseSeats(const std::string& in) {
    std::vector<Position> out;
    std::string cur;
    std::string norm = in;
    for (char& ch : norm) if (ch == ',' || ch == ';') ch = ' ';
    std::istringstream is(norm);
    while (is >> cur) {
        if (cur.size() < 2 || !std::isalpha(static_cast<unsigned char>(cur[0]))) throw BoxError("Ghế \"" + cur + "\" sai cú pháp, ví dụ: D3 D4.");
        int n = 0;
        try { std::size_t used = 0; n = std::stoi(cur.substr(1), &used); if (used != cur.size() - 1) n = 0; } catch (const std::exception&) { n = 0; }
        if (n < 1) throw BoxError("Ghế \"" + cur + "\" sai cú pháp, ví dụ: D3 D4.");
        out.push_back({std::toupper(static_cast<unsigned char>(cur[0])) - 'A', n - 1});
    }
    return out;
}

static std::string seatList(const BoxOffice& box, const Order& o) {
    std::string s;
    for (const auto& t : box.ticketsOf(o)) {
        if (t.status == TicketStatus::Cancelled && o.status == OrderStatus::Paid) continue;
        s += (s.empty() ? "" : " ") + SeatMap::seatName(t.row, t.col);
    }
    return s.empty() ? "-" : s;
}

// ======================= phim và suất chiếu =======================
static bool showVisible(const BoxOffice& box, const Showtime& s, std::time_t now) {
    const Movie* m = box.findMovie(s.movieId);
    return s.start + (m ? m->minutes : 120) * 60 > now;
}

static void listMovies(const Ctx& c) {
    title("PHIM");
    std::cout << "  " << C(col::bold, pad("ID", 4) + pad("Tên phim", 40) + pad("Thể loại", 20) + pad("Phút", 6) + "Tuổi   Trạng thái") << "\n";
    for (const auto& m : c.box.movies())
        std::cout << "  " << pad(std::to_string(m.id), 4) << pad(m.title, 40) << pad(m.genre, 20) << pad(std::to_string(m.minutes), 6)
                  << pad(m.rating, 7) << (m.status == "NOW" ? C(col::green, "Đang chiếu") : C(col::yellow, "Sắp chiếu")) << "\n";
}

static void listShows(const Ctx& c, int movieId, std::time_t now) {
    const Movie* m = c.box.findMovie(movieId);
    title("SUẤT CHIẾU: " + (m ? m->title : "?"));
    if (m && !m->synopsis.empty()) note(m->synopsis);
    std::cout << "  " << C(col::bold, pad("ID", 5) + pad("Giờ chiếu", 13) + pad("Phòng", 10) + pad("Giá thường", 13) + pad("Giá VIP", 13) + "Trống T/VIP   Chờ  Bán") << "\n";
    int n = 0;
    for (const auto& s : c.box.showtimes()) {
        if (s.movieId != movieId || !showVisible(c.box, s, now)) continue;
        const SeatMap& sm = c.box.seatMap(s.id);
        const bool open = c.box.salesOpen(s, now) && m && m->status == "NOW";
        std::cout << "  " << pad(std::to_string(s.id), 5) << pad(dateTime(s.start), 13) << pad(s.room, 10) << pad(vnd(s.priceNormal), 13) << pad(vnd(s.priceVip), 13)
                  << pad(std::to_string(sm.freeCount(SeatType::Normal)) + "/" + std::to_string(sm.freeCount(SeatType::Vip)), 14)
                  << pad(std::to_string(c.box.queue(s.id).size()), 5) << (open ? C(col::green, "mở") : C(col::gray, "đóng")) << "\n";
        ++n;
    }
    if (!n) warn("Phim chưa có suất chiếu nào sắp tới.");
}

// Chọn phim rồi chọn suất. Trả về 0 nếu người dùng bỏ qua.
static int pickShow(Ctx& c) {
    const std::time_t now = std::time(nullptr);
    listMovies(c);
    const int mid = static_cast<int>(askNumber("Nhập ID phim (0 = quay lại):", 0, 100000));
    if (!mid) return 0;
    if (!c.box.findMovie(mid)) { fail("Không có phim này."); return 0; }
    listShows(c, mid, now);
    const int sid = static_cast<int>(askNumber("Nhập ID suất chiếu (0 = quay lại):", 0, 100000));
    if (!sid) return 0;
    const Showtime* s = c.box.findShowtime(sid);
    if (!s || s->movieId != mid) { fail("Suất chiếu không thuộc phim này."); return 0; }
    c.showId = sid;
    return sid;
}

static void actViewSeats(Ctx& c) {
    if (pickShow(c)) drawSeats(c, c.showId);
}

// ======================= đơn hàng =======================
static std::string statusText(const Order& o, std::time_t now) {
    switch (o.status) {
        case OrderStatus::Hold: {
            const long left = static_cast<long>(o.holdUntil - now);
            char b[48];
            std::snprintf(b, sizeof b, "Chờ thanh toán, còn %02ld:%02ld", std::max(0L, left) / 60, std::max(0L, left) % 60);
            return C(col::yellow, b);
        }
        case OrderStatus::Paid: return o.checkedInAt ? C(col::cyan, "Đã soát vé lúc " + hhmm(o.checkedInAt)) : C(col::green, "Đã thanh toán");
        case OrderStatus::Cancelled: return C(col::gray, o.refund ? "Đã hủy, hoàn " + vnd(o.refund) : "Đã hủy");
        default: return C(col::gray, "Hết hạn giữ ghế");
    }
}

static void printOrder(const Ctx& c, const Order& o, std::time_t now) {
    const Showtime* s = c.box.findShowtime(o.showtimeId);
    const Movie* m = s ? c.box.findMovie(s->movieId) : nullptr;
    std::cout << "  " << C(col::bold, BoxOffice::orderCode(o.id)) << "  " << (m ? m->title : "?") << "\n"
              << "     " << (s ? dateTime(s->start) + " · " + s->room : "") << " · Ghế: " << C(col::bold, seatList(c.box, o))
              << " · Người nhận: " << o.name << (o.fromQueue ? C(col::magenta, " (từ hàng chờ)") : "") << "\n";
    if (!o.combos.empty()) {
        std::string cs;
        for (const auto& [id, q] : o.combos) {
            std::string nm = id;
            for (const auto& cb : c.box.combos()) if (cb.id == id) nm = cb.name;
            cs += (cs.empty() ? "" : ", ") + std::to_string(q) + "x " + nm;
        }
        std::cout << "     Bắp nước: " << cs << "\n";
    }
    std::cout << "     Tổng: " << C(col::bold, vnd(o.total));
    if (o.discount) std::cout << " (đã giảm " << vnd(o.discount) << (o.promo.empty() ? "" : " mã " + o.promo) << ")";
    if (!o.method.empty()) std::cout << " · " << o.method;
    std::cout << "\n     " << statusText(o, now) << "\n";
}

static void printQuote(const Ctx& c, const OrderRequest& rq, const Quote& q) {
    const Showtime* s = c.box.findShowtime(rq.showtimeId);
    std::string seats;
    for (const auto& p : q.seats) seats += (seats.empty() ? "" : " ") + SeatMap::seatName(p.row, p.col);
    std::cout << "\n  " << C(col::bold, "Tóm tắt đơn") << "\n"
              << "    Ghế (" << q.seats.size() << "):   " << seats << "\n"
              << "    Tiền vé:     " << padLeft(vnd(q.ticketsTotal), 12) << "\n";
    if (q.comboTotal) std::cout << "    Bắp nước:    " << padLeft(vnd(q.comboTotal), 12) << "\n";
    if (q.discount) std::cout << "    Giảm giá:    " << padLeft("-" + vnd(q.discount), 12) << "  " << C(col::green, q.promoNote) << "\n";
    std::cout << "    " << C(col::bold, "Thành tiền:  " + padLeft(vnd(q.total), 12)) << "\n";
    (void)s;
}

static void payFlow(Ctx& c, int orderId) {
    const std::time_t now = std::time(nullptr);
    const Order* o = c.box.findOrder(orderId);
    if (!o) { fail("Không tìm thấy đơn."); return; }
    if (!c.admin() && o->owner != c.user->username) { fail("Bạn chỉ thanh toán được đơn của chính mình."); return; }
    if (o->status != OrderStatus::Hold) { fail("Đơn này không ở trạng thái chờ thanh toán."); return; }
    std::cout << "\n  Số tiền cần thanh toán: " << C(col::bold, vnd(o->total)) << "\n";
    if (c.admin()) std::cout << "    1. Tiền mặt tại quầy\n";
    std::cout << "    2. Chuyển khoản (mô phỏng)\n    3. Ví điện tử (mô phỏng)\n    4. Thẻ ngân hàng (mô phỏng)\n    0. Để sau\n";
    const long ch = askNumber("Chọn phương thức:", 0, 4);
    if (ch == 0) { note("Đơn được giữ ghế đến khi hết hạn, bạn có thể thanh toán ở mục \"Đơn của tôi\"."); return; }
    if (ch == 1 && !c.admin()) { fail("Tiền mặt chỉ thanh toán tại quầy."); return; }
    const std::string method = ch == 1 ? "CASH" : ch == 2 ? "BANK" : ch == 3 ? "WALLET" : "CARD";
    long cash = 0;
    if (ch == 1) cash = askNumber("Tiền khách đưa (đồng):", 0, 1000000000);
    else if (!askYes("Xác nhận thanh toán " + vnd(o->total) + " bằng " + method + "?")) return;
    const auto p = c.box.pay(orderId, method, cash, c.admin(), now);
    c.box.save();
    ok("Thanh toán thành công. Mã vé: " + BoxOffice::orderCode(p.order.id));
    if (p.change) std::cout << "  Tiền thối: " << C(col::bold, vnd(p.change)) << "\n";
    if (!p.order.payRef.empty() && p.order.payRef != "CASH") note("Mã giao dịch: " + p.order.payRef);
}

static void joinQueueFlow(Ctx& c, int showId, SeatType type) {
    const std::time_t now = std::time(nullptr);
    std::string name = c.admin() ? ask("Tên khách:") : ask("Tên người nhận vé [" + c.user->fullName + "]:");
    if (name.empty() && !c.admin()) name = c.user->fullName;
    const auto j = c.box.joinQueue(showId, type, name, ownerOf(c), now);
    c.box.save();
    ok("Đã vào hàng chờ, vị trí số " + std::to_string(j.position) + (type == SeatType::Vip ? " (VIP được ưu tiên lên đầu hàng)." : "."));
    note("Khi có ghế trống bạn sẽ được giữ ghế 10 phút, xem ở mục \"Đơn của tôi\".");
}

static void actBook(Ctx& c) {
    const std::time_t now = std::time(nullptr);
    if (!c.loggedIn()) return;
    if (!pickShow(c)) return;
    const Showtime* st = c.box.findShowtime(c.showId);
    const Movie* mv = c.box.findMovie(st->movieId);
    if (mv && mv->status != "NOW") { fail("Phim này chưa mở bán vé."); return; }
    if (!c.box.salesOpen(*st, now)) { fail("Suất chiếu đã ngừng bán vé."); return; }
    if (mv && mv->rating != "P") {
        const std::string age = mv->rating.substr(1);
        if (!askYes("Phim giới hạn độ tuổi " + mv->rating + ". Người xem đủ " + age + " tuổi trở lên?", false)) { warn("Đã hủy đặt vé."); return; }
    }
    drawSeats(c, c.showId);

    OrderRequest rq;
    rq.showtimeId = c.showId;
    rq.owner = ownerOf(c);
    std::cout << "\n    1. Tự chọn ghế trên sơ đồ\n    2. Hệ thống chọn ghế liền kề gần giữa phòng\n    3. Hết ghế, vào hàng chờ\n    0. Quay lại\n";
    const long mode = askNumber("Chọn:", 0, 3);
    if (mode == 0) return;
    if (mode == 3) {
        const long t = askNumber("Loại ghế muốn chờ (1 = Thường, 2 = VIP):", 1, 2);
        joinQueueFlow(c, c.showId, t == 2 ? SeatType::Vip : SeatType::Normal);
        return;
    }
    if (mode == 1) {
        while (true) {
            const std::string in = ask("Nhập ghế, cách nhau bằng dấu cách (VD: D3 D4), để trống để quay lại:");
            if (in.empty()) return;
            try {
                rq.seats = parseSeats(in);
                (void)c.box.quote(rq, now);
                break;
            } catch (const BoxError& e) { fail(e.what()); }
        }
        drawSeats(c, c.showId, rq.seats);
    } else {
        rq.autoType = askNumber("Loại ghế (1 = Thường, 2 = VIP):", 1, 2) == 2 ? SeatType::Vip : SeatType::Normal;
        rq.autoCount = static_cast<int>(askNumber("Số ghế (1-" + std::to_string(MAX_SEATS_PER_ORDER) + "):", 1, MAX_SEATS_PER_ORDER));
        try {
            const Quote q = c.box.quote(rq, now);
            rq.seats = q.seats;
            rq.autoCount = 0;
            drawSeats(c, c.showId, rq.seats);
        } catch (const BoxError& e) {
            fail(e.what());
            if (c.box.seatMap(c.showId).freeCount(rq.autoType) == 0 && askYes("Loại ghế này đã hết. Vào hàng chờ?")) joinQueueFlow(c, c.showId, rq.autoType);
            return;
        }
    }

    std::string name = c.admin() ? ask("Tên khách:") : ask("Tên người nhận vé [" + c.user->fullName + "]:");
    if (name.empty() && !c.admin()) name = c.user->fullName;
    rq.name = name;

    if (askYes("Thêm bắp nước?", false)) {
        const auto& cb = c.box.combos();
        for (std::size_t i = 0; i < cb.size(); ++i)
            std::cout << "    " << (i + 1) << ". " << pad(cb[i].name, 28) << vnd(cb[i].price) << "\n";
        for (const auto& item : cb) {
            const long q = askNumber(item.name + " - số lượng (Enter = 0):", 0, 10, 0);
            if (q > 0) rq.combos[item.id] = static_cast<int>(q);
        }
    }
    if (askYes("Có mã giảm giá?", false)) {
        for (const auto& p : c.box.promos()) std::cout << "    " << pad(p.code, 9) << p.desc << "\n";
        rq.promo = ask("Nhập mã:");
    }

    Quote q;
    try { q = c.box.quote(rq, now); } catch (const BoxError& e) { fail(e.what()); return; }
    printQuote(c, rq, q);
    if (!askYes("Giữ ghế (10 phút để thanh toán)?")) { warn("Đã hủy đặt vé."); return; }
    const Order o = c.box.createOrder(rq, now);
    c.box.save();
    ok("Đã giữ ghế " + seatList(c.box, o) + ", mã đơn " + BoxOffice::orderCode(o.id) + ". Hết hạn lúc " + hhmm(o.holdUntil) + ".");
    if (askYes("Thanh toán ngay?")) payFlow(c, o.id);
}

static std::vector<const Order*> visibleOrders(const Ctx& c) {
    std::vector<const Order*> v;
    for (const auto& o : c.box.orders()) if (c.admin() || (!o.owner.empty() && o.owner == c.user->username)) v.push_back(&o);
    std::sort(v.begin(), v.end(), [](const Order* a, const Order* b) { return a->id > b->id; });
    return v;
}

static void actMyOrders(Ctx& c) {
    const std::time_t now = std::time(nullptr);
    title(c.admin() ? "TẤT CẢ ĐƠN HÀNG" : "ĐƠN CỦA TÔI");
    auto v = visibleOrders(c);
    if (v.empty()) { note("Chưa có đơn hàng nào."); return; }
    std::size_t limit = c.admin() ? 15 : v.size();
    if (v.size() > limit) note("Hiển thị " + std::to_string(limit) + " đơn mới nhất trong " + std::to_string(v.size()) + " đơn.");
    for (std::size_t i = 0; i < std::min(limit, v.size()); ++i) { printOrder(c, *v[i], now); std::cout << "\n"; }
}

static int askOrderId(const Ctx& c, const std::string& prompt) {
    const std::string code = ask(prompt);
    if (code.empty()) return 0;
    const int id = BoxOffice::parseOrderCode(code);
    if (!id) { fail("Mã đơn không hợp lệ, mã có dạng CQ000123."); return 0; }
    const Order* o = c.box.findOrder(id);
    if (!o) { fail("Không tìm thấy đơn hàng."); return 0; }
    if (!c.admin() && o->owner != c.user->username) { fail("Đây không phải đơn của bạn."); return 0; }
    return id;
}

static void actPay(Ctx& c) {
    actMyOrders(c);
    const int id = askOrderId(c, "Nhập mã đơn cần thanh toán (để trống = quay lại):");
    if (id) payFlow(c, id);
}

static void actCancel(Ctx& c) {
    actMyOrders(c);
    const int id = askOrderId(c, "Nhập mã đơn cần hủy (để trống = quay lại):");
    if (!id) return;
    const Order* o = c.box.findOrder(id);
    if (o->status == OrderStatus::Paid)
        warn("Đơn đã thanh toán, hủy sẽ được hoàn " + std::to_string(REFUND_PERCENT) + "% = " + vnd(o->total * REFUND_PERCENT / 100) + ".");
    if (!askYes("Xác nhận hủy đơn " + BoxOffice::orderCode(id) + "?", false)) return;
    const auto r = c.box.cancel(id, c.admin(), std::time(nullptr));
    c.box.save();
    ok("Đã hủy đơn." + (r.refund ? " Hoàn tiền " + vnd(r.refund) + "." : std::string()));
    for (const auto& g : r.granted)
        std::cout << "  " << C(col::magenta, "→ Ghế trống được giao cho " + g.name + " trong hàng chờ (đơn " + BoxOffice::orderCode(g.id) + ", giữ 10 phút).") << "\n";
}

// ======================= hàng chờ =======================
static void printQueue(const Ctx& c, int showId) {
    const auto q = c.box.queue(showId);
    if (q.empty()) { note("Hàng chờ trống (HEAD = null, TAIL = null)."); return; }
    std::cout << "\n  " << C(col::bold, "HEAD") << " ";
    for (std::size_t i = 0; i < q.size(); ++i) {
        if (i) std::cout << " ⇄ ";
        std::string who = c.admin() || (c.user && q[i].owner == c.user->username) ? q[i].name : "Khách";
        std::cout << C(q[i].type == SeatType::Vip ? col::yellow : col::green, "#" + std::to_string(q[i].id) + " " + who + (q[i].type == SeatType::Vip ? " (VIP)" : ""));
    }
    std::cout << " " << C(col::bold, "TAIL") << "\n";
    note("VIP chen lên đầu hàng (pushFront), khách thường vào cuối (pushBack).");
}

static void actQueue(Ctx& c) {
    if (!pickShow(c)) return;
    printQueue(c, c.showId);
    if (!c.loggedIn()) return;
    std::cout << "\n    1. Xin vào hàng chờ\n    2. Rời hàng chờ\n    0. Quay lại\n";
    const long ch = askNumber("Chọn:", 0, 2);
    if (ch == 1) {
        const long t = askNumber("Loại ghế muốn chờ (1 = Thường, 2 = VIP):", 1, 2);
        joinQueueFlow(c, c.showId, t == 2 ? SeatType::Vip : SeatType::Normal);
        printQueue(c, c.showId);
    } else if (ch == 2) {
        const int cid = static_cast<int>(askNumber("Nhập số thứ tự # của khách cần rời hàng:", 0, 1000000));
        if (!cid) return;
        c.box.leaveQueue(c.showId, cid, ownerOf(c), c.admin());
        c.box.save();
        ok("Đã rời hàng chờ.");
    }
}

// ======================= tin tức =======================
static void actNews(Ctx& c) {
    title("TIN TỨC");
    const auto items = c.news.list();
    if (items.empty()) { note("Chưa có tin."); return; }
    for (const auto& n : items) {
        std::cout << "  " << (n.pinned ? C(col::yellow, "📌 ") : "") << C(col::bold, "#" + std::to_string(n.id) + " " + n.title) << "  " << C(col::cyan, "[" + n.category + "]")
                  << C(col::gray, " " + dateStr(static_cast<std::time_t>(n.ts))) << "\n";
        if (!n.movie.empty()) std::cout << "     Phim: " << n.movie << "\n";
        std::istringstream body(n.body);
        std::string line;
        while (std::getline(body, line)) std::cout << "     " << line << "\n";
        std::cout << "\n";
    }
}

// ======================= đăng nhập / đăng ký =======================
static bool actLogin(Ctx& c) {
    title("ĐĂNG NHẬP");
    const std::string u = ask("Tên đăng nhập:");
    if (g_eof) return false;
    const std::string p = askPassword("Mật khẩu:");
    const AuthResult r = c.auth.login(u, p);
    if (!r.ok) { fail(r.error); return false; }
    c.user = r.user;
    ok("Xin chào " + r.user.fullName + " (" + (r.user.isAdmin() ? "Quản trị" : "Khách hàng") + ").");
    return true;
}

static void actRegister(Ctx& c) {
    title("ĐĂNG KÝ TÀI KHOẢN");
    const std::string full = ask("Họ và tên:");
    const std::string user = ask("Tên đăng nhập:");
    const std::string p1 = askPassword("Mật khẩu:");
    const std::string p2 = askPassword("Nhập lại mật khẩu:");
    const AuthResult r = c.auth.registerUser(full, user, p1, p2);
    if (!r.ok) { fail(r.error); return; }
    c.user = r.user;
    ok("Đăng ký thành công, đã đăng nhập với tên " + r.user.username + ".");
}

// ======================= quản trị =======================
static void actCheckIn(Ctx& c) {
    title("SOÁT VÉ");
    const std::string code = ask("Nhập mã vé (VD CQ000012 hoặc CQ000012-D3):");
    if (code.empty()) return;
    const auto p = c.box.checkIn(code, std::time(nullptr));
    c.box.save();
    ok("Soát vé thành công.");
    printOrder(c, p.order, std::time(nullptr));
}

static void actStats(Ctx& c) {
    const Stats s = c.box.stats();
    title("THỐNG KÊ");
    std::cout << "  Doanh thu gộp:  " << padLeft(vnd(s.gross), 14) << "\n  Hoàn tiền:      " << padLeft(vnd(s.refunds), 14) << "\n  "
              << C(col::bold, "Doanh thu thuần:" ) << " " << C(col::bold, padLeft(vnd(s.net), 14)) << "\n\n"
              << "  Vé đã bán: " << s.ticketsSold << " · Đơn đã trả tiền: " << s.ordersPaid << " · Đang giữ: " << s.ordersHold << " · Đã hủy: " << s.ordersCancelled
              << " · Hết hạn: " << s.ordersExpired << "\n  Đã soát vé: " << s.checkedIn << " · Đang trong hàng chờ: " << s.waiting << "\n\n";
    std::cout << "  " << C(col::bold, pad("Phim", 42) + padLeft("Vé", 5) + padLeft("Doanh thu", 14)) << "\n";
    for (const auto& m : s.movies) std::cout << "  " << pad(m.title, 42) << padLeft(std::to_string(m.tickets), 5) << padLeft(vnd(m.revenue), 14) << "\n";
    if (!s.combos.empty()) {
        std::cout << "\n  " << C(col::bold, "Bắp nước đã bán") << "\n";
        for (const auto& [id, q] : s.combos) {
            std::string nm = id;
            for (const auto& cb : c.box.combos()) if (cb.id == id) nm = cb.name;
            std::cout << "  " << pad(nm, 30) << q << "\n";
        }
    }
    const std::time_t now = std::time(nullptr);
    std::cout << "\n  " << C(col::bold, "Mức lấp đầy các suất sắp chiếu") << "\n";
    int n = 0;
    for (const auto& sh : s.shows) {
        const Showtime* st = c.box.findShowtime(sh.showtimeId);
        if (!st || !showVisible(c.box, *st, now) || n >= 12) continue;
        const Movie* m = c.box.findMovie(st->movieId);
        const int pct = sh.total ? sh.taken * 100 / sh.total : 0;
        const int bar = pct / 5;
        std::cout << "  #" << pad(std::to_string(st->id), 4) << pad(dateTime(st->start), 13) << pad(m ? m->title : "?", 36) << C(col::green, rep("█", bar)) << C(col::gray, rep("░", 20 - bar))
                  << " " << pct << "% (" << sh.taken << "/" << sh.total << ")\n";
        ++n;
    }
}

static std::time_t parseDateTime(const std::string& s) {
    int d = 0, mo = 0, y = 0, h = 0, mi = 0;
    if (std::sscanf(s.c_str(), "%d/%d/%d %d:%d", &d, &mo, &y, &h, &mi) != 5) return 0;
    std::tm t{};
    t.tm_mday = d; t.tm_mon = mo - 1; t.tm_year = y - 1900; t.tm_hour = h; t.tm_min = mi; t.tm_isdst = -1;
    return std::mktime(&t);
}

static void actManageMovies(Ctx& c) {
    while (!g_eof) {
        listMovies(c);
        std::cout << "\n    1. Thêm phim\n    2. Xóa phim\n    3. Thêm suất chiếu\n    4. Xóa suất chiếu\n    0. Quay lại\n";
        const long ch = askNumber("Chọn:", 0, 4);
        try {
            if (ch == 0) return;
            if (ch == 1) {
                Movie m;
                m.title = ask("Tên phim:");
                m.genre = ask("Thể loại:");
                m.minutes = static_cast<int>(askNumber("Thời lượng (phút):", 30, 300));
                const long r = askNumber("Độ tuổi (1 = P, 2 = T13, 3 = T16, 4 = T18):", 1, 4);
                m.rating = r == 1 ? "P" : r == 2 ? "T13" : r == 3 ? "T16" : "T18";
                m.status = askNumber("Trạng thái (1 = Đang chiếu, 2 = Sắp chiếu):", 1, 2) == 1 ? "NOW" : "SOON";
                m.synopsis = ask("Mô tả ngắn:");
                const int id = c.box.saveMovie(m);
                c.box.save();
                ok("Đã thêm phim #" + std::to_string(id) + ".");
            } else if (ch == 2) {
                const int id = static_cast<int>(askNumber("ID phim cần xóa (0 = hủy):", 0, 100000));
                if (id && askYes("Xóa phim #" + std::to_string(id) + "?", false)) { c.box.removeMovie(id); c.box.save(); ok("Đã xóa phim."); }
            } else if (ch == 3) {
                Showtime s;
                s.movieId = static_cast<int>(askNumber("ID phim:", 1, 100000));
                s.room = ask("Tên phòng (VD Phòng 1):");
                while (!g_eof) {
                    s.start = parseDateTime(ask("Giờ chiếu (dd/mm/yyyy hh:mm):"));
                    if (s.start) break;
                    warn("Sai định dạng, ví dụ 28/10/2026 19:30.");
                }
                s.rows = static_cast<int>(askNumber("Số hàng ghế (1-15):", 1, 15));
                s.cols = static_cast<int>(askNumber("Số ghế mỗi hàng (2-20):", 2, 20));
                s.vipRows = static_cast<int>(askNumber("Số hàng VIP (tính từ hàng D):", 0, s.rows));
                s.priceNormal = askNumber("Giá vé thường (đồng):", 1000, 10000000);
                s.priceVip = askNumber("Giá vé VIP (đồng):", 1000, 10000000);
                const int id = c.box.saveShowtime(s);
                c.box.save();
                ok("Đã thêm suất chiếu #" + std::to_string(id) + ".");
            } else if (ch == 4) {
                const int id = static_cast<int>(askNumber("ID suất cần xóa (0 = hủy):", 0, 100000));
                if (id && askYes("Xóa suất #" + std::to_string(id) + "?", false)) { c.box.removeShowtime(id); c.box.save(); ok("Đã xóa suất chiếu."); }
            }
        } catch (const BoxError& e) { fail(e.what()); }
    }
}

static void actPostNews(Ctx& c) {
    title("ĐĂNG / XÓA TIN");
    std::cout << "    1. Đăng tin mới\n    2. Xóa tin\n    0. Quay lại\n";
    const long ch = askNumber("Chọn:", 0, 2);
    if (ch == 1) {
        NewsInput in;
        const auto& cats = newsCategories();
        for (std::size_t i = 0; i < cats.size(); ++i) std::cout << "    " << (i + 1) << ". " << cats[i] << "\n";
        in.category = cats[static_cast<std::size_t>(askNumber("Chọn chuyên mục:", 1, static_cast<long>(cats.size()))) - 1];
        in.title = ask("Tiêu đề:");
        in.body = ask("Nội dung:");
        in.movie = ask("Phim liên quan (có thể để trống):");
        in.pinned = askYes("Ghim lên đầu?", false);
        const NewsItem n = c.news.create(in, c.user->username);
        ok("Đã đăng tin #" + std::to_string(n.id) + ".");
    } else if (ch == 2) {
        const int id = static_cast<int>(askNumber("ID tin cần xóa (0 = hủy):", 0, 100000));
        if (id) { c.news.remove(id); ok("Đã xóa tin."); }
    }
}

static void actReset(Ctx& c) {
    warn("Thao tác này xóa toàn bộ vé, đơn hàng và hàng chờ (giữ phim và suất chiếu).");
    if (ask("Gõ XOA để xác nhận:") != "XOA") { note("Đã hủy."); return; }
    c.box.resetSales();
    c.box.save();
    ok("Đã đặt lại dữ liệu bán vé.");
}

// ======================= menu =======================
struct Item { std::string label; std::function<void()> fn; };

static void banner(const Ctx& c) {
    std::cout << "\n" << C(col::cyan, C(col::bold, "  ╔══════════════════════════════════════╗")) << "\n"
              << C(col::cyan, C(col::bold, "  ║")) << C(col::bold, "   CineQ · Quầy vé rạp chiếu phim     ") << C(col::cyan, C(col::bold, "║")) << "\n"
              << C(col::cyan, C(col::bold, "  ╚══════════════════════════════════════╝")) << "\n";
    if (c.user) std::cout << "  Đang đăng nhập: " << C(col::bold, c.user->fullName) << " " << C(c.user->isAdmin() ? col::yellow : col::green, c.user->isAdmin() ? "[Quản trị]" : "[Khách hàng]") << "\n";
    else std::cout << "  " << C(col::gray, "Chế độ khách: xem tự do, đăng nhập khi cần đặt vé") << "\n";
}

static std::vector<Item> menuFor(Ctx& c) {
    std::vector<Item> m;
    m.push_back({"Phim, suất chiếu và sơ đồ ghế", [&] { actViewSeats(c); }});
    m.push_back({"Tin tức", [&] { actNews(c); }});
    m.push_back({"Hàng chờ (Deque)", [&] { actQueue(c); }});
    if (!c.loggedIn()) {
        m.push_back({"Đặt vé (cần đăng nhập)", [&] { if (actLogin(c)) actBook(c); }});
        m.push_back({"Đăng nhập", [&] { actLogin(c); }});
        m.push_back({"Đăng ký tài khoản", [&] { actRegister(c); }});
    } else if (!c.admin()) {
        m.insert(m.begin() + 2, {"Đặt vé", [&] { actBook(c); }});
        m.push_back({"Đơn của tôi", [&] { actMyOrders(c); }});
        m.push_back({"Thanh toán đơn", [&] { actPay(c); }});
        m.push_back({"Hủy đơn", [&] { actCancel(c); }});
        m.push_back({"Đăng xuất", [&] { c.user.reset(); ok("Đã đăng xuất."); }});
    } else {
        m.insert(m.begin() + 2, {"Bán vé tại quầy", [&] { actBook(c); }});
        m.push_back({"Danh sách đơn hàng", [&] { actMyOrders(c); }});
        m.push_back({"Thanh toán đơn (tiền mặt, thẻ...)", [&] { actPay(c); }});
        m.push_back({"Hủy đơn / hoàn tiền", [&] { actCancel(c); }});
        m.push_back({"Soát vé", [&] { actCheckIn(c); }});
        m.push_back({"Thống kê doanh thu", [&] { actStats(c); }});
        m.push_back({"Quản lý phim và suất chiếu", [&] { actManageMovies(c); }});
        m.push_back({"Đăng / xóa tin tức", [&] { actPostNews(c); }});
        m.push_back({"Đặt lại dữ liệu bán vé", [&] { actReset(c); }});
        m.push_back({"Đăng xuất", [&] { c.user.reset(); ok("Đã đăng xuất."); }});
    }
    return m;
}

int main() {
    setupConsole();
    BoxOffice box("data");
    try {
        box.load(std::time(nullptr));
    } catch (const std::exception& e) {
        std::cerr << "Không nạp được dữ liệu: " << e.what() << "\n";
        return 1;
    }
    AuthService auth("data/users.csv");
    NewsService news("data/news.csv");
    Ctx c{box, auth, news, std::nullopt, 0};

    while (!g_eof) {
        if (box.sweep(std::time(nullptr))) box.save();  // nhả ghế các đơn hết hạn giữ, giao cho hàng chờ
        banner(c);
        const auto menu = menuFor(c);
        std::cout << "\n";
        for (std::size_t i = 0; i < menu.size(); ++i) std::cout << "   " << C(col::cyan, padLeft(std::to_string(i + 1), 2) + ".") << " " << menu[i].label << "\n";
        std::cout << "   " << C(col::cyan, " 0.") << " Thoát\n";
        const long ch = askNumber("Chọn:", 0, static_cast<long>(menu.size()));
        if (g_eof || ch == 0) break;
        try {
            menu[static_cast<std::size_t>(ch) - 1].fn();
        } catch (const BoxError& e) {
            fail(e.what());
        } catch (const std::exception& e) {
            fail(e.what());
        }
        if (!g_eof) ask("Nhấn Enter để tiếp tục...");
    }
    std::cout << "\nTạm biệt!\n";
    return 0;
}


