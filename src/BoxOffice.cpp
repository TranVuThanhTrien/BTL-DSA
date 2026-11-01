#include "BoxOffice.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

#include "../storage/Storage.h"

using std::string;

string toString(OrderStatus s) {
    switch (s) {
        case OrderStatus::Paid: return "PAID";
        case OrderStatus::Cancelled: return "CANCELLED";
        case OrderStatus::Expired: return "EXPIRED";
        default: return "HOLD";
    }
}
OrderStatus orderStatusFromString(const string& s) {
    if (s == "PAID") return OrderStatus::Paid;
    if (s == "CANCELLED") return OrderStatus::Cancelled;
    if (s == "EXPIRED") return OrderStatus::Expired;
    if (s == "HOLD") return OrderStatus::Hold;
    throw StorageError("Trang thai don hang khong hop le: '" + s + "'");
}

static string upper(string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
static long toLong(const string& s, const char* what) {
    try { return std::stol(s); } catch (...) { throw StorageError(string("Gia tri khong hop le o cot ") + what + ": '" + s + "'"); }
}

// ---------- danh mục cố định ----------
const std::vector<Combo>& BoxOffice::combos() const {
    static const std::vector<Combo> v{
        {"BAP", "Bắp ngọt (vừa)", 45000},
        {"BAPPM", "Bắp phô mai (vừa)", 55000},
        {"NUOC", "Nước ngọt (ly lớn)", 30000},
        {"C1", "Combo 1: 1 bắp + 1 nước", 69000},
        {"C2", "Combo 2: 1 bắp + 2 nước", 89000},
    };
    return v;
}
const std::vector<Promo>& BoxOffice::promos() const {
    static const std::vector<Promo> v{
        {"CINE10", 10, 30000, 150000, "Giảm 10%, tối đa 30.000 đ, đơn vé từ 150.000 đ"},
        {"HSSV15", 15, 40000, 0, "Giảm 15%, tối đa 40.000 đ, dành cho học sinh sinh viên"},
        {"VUIVE20", 20, 60000, 300000, "Giảm 20%, tối đa 60.000 đ, đơn vé từ 300.000 đ"},
    };
    return v;
}

BoxOffice::BoxOffice(string dir) : dir_(std::move(dir)) {}

// ---------- tra cứu ----------
const Movie* BoxOffice::findMovie(int id) const {
    for (const auto& m : movies_) if (m.id == id) return &m;
    return nullptr;
}
const Showtime* BoxOffice::findShowtime(int id) const {
    for (const auto& s : showtimes_) if (s.id == id) return &s;
    return nullptr;
}
BoxOffice::Show& BoxOffice::showOf(int id) {
    for (auto& s : shows_) if (s->info.id == id) return *s;
    throw BoxError("Không tìm thấy suất chiếu.", 404);
}
const BoxOffice::Show& BoxOffice::showOf(int id) const {
    for (const auto& s : shows_) if (s->info.id == id) return *s;
    throw BoxError("Không tìm thấy suất chiếu.", 404);
}
Order* BoxOffice::order(int id) {
    for (auto& o : orders_) if (o.id == id) return &o;
    return nullptr;
}
const Order* BoxOffice::findOrder(int id) const {
    for (const auto& o : orders_) if (o.id == id) return &o;
    return nullptr;
}
const SeatMap& BoxOffice::seatMap(int id) const { return showOf(id).seats; }
std::vector<Customer> BoxOffice::queue(int id) const { return showOf(id).service.queueSnapshot(); }
bool BoxOffice::salesOpen(const Showtime& s, std::time_t now) const {
    return now < s.start - SALES_CLOSE_MIN * 60;
}
std::vector<Ticket> BoxOffice::ticketsOf(const Order& o) const {
    std::vector<Ticket> out;
    for (const auto& sh : shows_) {
        if (sh->info.id != o.showtimeId) continue;
        for (int id : o.ticketIds)
            if (const Ticket* t = sh->service.findTicket(id)) out.push_back(*t);
    }
    return out;
}
string BoxOffice::orderCode(int id) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "CQ%06d", id);
    return buf;
}
int BoxOffice::parseOrderCode(const string& raw, string* seat) {
    string s = upper(raw);
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
    if (s.size() < 3 || s.compare(0, 2, "CQ") != 0) return 0;
    std::size_t i = 2;
    int id = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) && id < 100000000) id = id * 10 + (s[i++] - '0');
    if (i == 2) return 0;
    if (i < s.size()) {
        if (s[i] != '-' || i + 1 >= s.size()) return 0;
        if (seat) *seat = s.substr(i + 1);
    }
    return id;
}

// ---------- danh mục: thêm sửa xóa ----------
int BoxOffice::saveMovie(Movie m) {
    m.title.erase(0, m.title.find_first_not_of(' '));
    while (!m.title.empty() && m.title.back() == ' ') m.title.pop_back();
    if (m.title.empty()) throw BoxError("Nhập tên phim.");
    if (m.minutes < 30 || m.minutes > 300) throw BoxError("Thời lượng phim phải từ 30 đến 300 phút.");
    if (m.rating != "P" && m.rating != "T13" && m.rating != "T16" && m.rating != "T18") throw BoxError("Nhãn độ tuổi không hợp lệ.");
    if (m.status != "NOW" && m.status != "SOON") throw BoxError("Trạng thái phim không hợp lệ.");
    if (m.id == 0) {
        m.id = nextMovieId_++;
        movies_.push_back(m);
        return m.id;
    }
    for (auto& x : movies_)
        if (x.id == m.id) { x = m; return m.id; }
    throw BoxError("Không tìm thấy phim.", 404);
}
void BoxOffice::removeMovie(int id) {
    for (const auto& s : showtimes_)
        if (s.movieId == id) throw BoxError("Phim còn suất chiếu, hãy xóa suất chiếu trước.");
    auto it = std::remove_if(movies_.begin(), movies_.end(), [id](const Movie& m) { return m.id == id; });
    if (it == movies_.end()) throw BoxError("Không tìm thấy phim.", 404);
    movies_.erase(it, movies_.end());
}

int BoxOffice::saveShowtime(Showtime s) {
    const Movie* mv = findMovie(s.movieId);
    if (!mv) throw BoxError("Chọn phim cho suất chiếu.");
    if (s.room.empty()) throw BoxError("Nhập tên phòng chiếu.");
    if (s.start <= 0) throw BoxError("Chọn giờ chiếu.");
    if (s.rows < 1 || s.rows > 15 || s.cols < 2 || s.cols > 20 || s.vipRows < 0 || s.vipRows > s.rows)
        throw BoxError("Kích thước phòng không hợp lệ (1-15 hàng, 2-20 ghế mỗi hàng).");
    if (s.priceNormal <= 0 || s.priceVip <= 0) throw BoxError("Giá vé phải lớn hơn 0.");
    for (const auto& o : showtimes_) {  // không trùng giờ trong cùng phòng, cách nhau 15 phút dọn phòng
        if (o.id == s.id || o.room != s.room) continue;
        const Movie* om = findMovie(o.movieId);
        const std::time_t oEnd = o.start + (om ? om->minutes : 120) * 60 + 15 * 60;
        const std::time_t sEnd = s.start + mv->minutes * 60 + 15 * 60;
        if (s.start < oEnd && o.start < sEnd) throw BoxError("Phòng " + s.room + " đã có suất chiếu trùng giờ.");
    }
    if (s.id == 0) {
        s.id = nextShowId_++;
        showtimes_.push_back(s);
        shows_.push_back(std::make_unique<Show>(s));
        return s.id;
    }
    for (auto& x : showtimes_) {
        if (x.id != s.id) continue;
        Show& sh = showOf(s.id);
        const bool layout = x.rows != s.rows || x.cols != s.cols || x.vipRows != s.vipRows;
        bool active = false;
        for (const auto& t : sh.service.tickets()) if (t.status != TicketStatus::Cancelled) active = true;
        if (layout && (active || !sh.service.queueSnapshot().empty()))
            throw BoxError("Suất đã có vé hoặc người chờ, không đổi được kích thước phòng.");
        x = s;
        if (layout) {
            for (auto& p : shows_) if (p->info.id == s.id) p = std::make_unique<Show>(s);
        } else {
            sh.info = s;
            sh.service.setPrices(s.priceNormal, s.priceVip);
        }
        return s.id;
    }
    throw BoxError("Không tìm thấy suất chiếu.", 404);
}
void BoxOffice::removeShowtime(int id) {
    for (const auto& o : orders_)
        if (o.showtimeId == id) throw BoxError("Suất chiếu đã có đơn hàng, không thể xóa.");
    const Show& sh = showOf(id);
    if (!sh.service.queueSnapshot().empty()) throw BoxError("Suất chiếu còn người chờ, không thể xóa.");
    showtimes_.erase(std::remove_if(showtimes_.begin(), showtimes_.end(), [id](const Showtime& s) { return s.id == id; }), showtimes_.end());
    shows_.erase(std::remove_if(shows_.begin(), shows_.end(), [id](const std::unique_ptr<Show>& p) { return p->info.id == id; }), shows_.end());
    std::remove(path("shows/tickets_" + std::to_string(id) + ".csv").c_str());
    std::remove(path("shows/queue_" + std::to_string(id) + ".csv").c_str());
}

// ---------- báo giá và đặt vé ----------
void BoxOffice::plan(const OrderRequest& req, std::time_t now, Quote& q) const {
    const Show& sh = showOf(req.showtimeId);
    const Movie* mv = findMovie(sh.info.movieId);
    if (!mv) throw BoxError("Suất chiếu không còn phim.");
    if (mv->status != "NOW") throw BoxError("Phim này chưa mở bán vé.");
    if (!salesOpen(sh.info, now)) throw BoxError("Suất chiếu đã ngừng bán vé.");

    if (!req.seats.empty()) {
        if (static_cast<int>(req.seats.size()) > MAX_SEATS_PER_ORDER)
            throw BoxError("Mỗi đơn đặt tối đa " + std::to_string(MAX_SEATS_PER_ORDER) + " ghế.");
        for (std::size_t i = 0; i < req.seats.size(); ++i) {
            const Position p = req.seats[i];
            if (!sh.seats.inRange(p.row, p.col)) throw BoxError("Ghế không tồn tại trong phòng.");
            if (!sh.seats.isFree(p.row, p.col)) throw BoxError("Ghế " + SeatMap::seatName(p.row, p.col) + " đã có người đặt.");
            for (std::size_t j = 0; j < i; ++j)
                if (req.seats[j].row == p.row && req.seats[j].col == p.col) throw BoxError("Ghế chọn bị trùng.");
        }
        q.seats = req.seats;
    } else {
        if (req.autoCount < 1 || req.autoCount > MAX_SEATS_PER_ORDER)
            throw BoxError("Chọn từ 1 đến " + std::to_string(MAX_SEATS_PER_ORDER) + " ghế.");
        q.seats = sh.seats.findBlock(req.autoType, req.autoCount);
        if (q.seats.empty())
            throw BoxError("Không đủ " + std::to_string(req.autoCount) + " ghế " + (req.autoType == SeatType::Vip ? "VIP" : "thường") + " còn trống.");
    }
    q.seats = q.seats;
    q.ticketsTotal = 0;
    for (const auto& p : q.seats) q.ticketsTotal += sh.service.priceOf(sh.seats.typeAt(p.row, p.col));

    q.comboTotal = 0;
    for (const auto& [id, qty] : req.combos) {
        if (qty == 0) continue;
        if (qty < 0 || qty > 10) throw BoxError("Số lượng bắp nước từ 1 đến 10.");
        const auto& all = combos();
        auto it = std::find_if(all.begin(), all.end(), [&](const Combo& c) { return c.id == id; });
        if (it == all.end()) throw BoxError("Món bắp nước không tồn tại.");
        q.comboTotal += it->price * qty;
    }

    q.discount = 0;
    q.promoNote.clear();
    if (!req.promo.empty()) {
        const string code = upper(req.promo);
        const auto& all = promos();
        auto it = std::find_if(all.begin(), all.end(), [&](const Promo& p) { return p.code == code; });
        if (it == all.end()) throw BoxError("Mã khuyến mãi không tồn tại hoặc đã hết hạn.");
        if (q.ticketsTotal < it->minTotal) throw BoxError("Mã " + code + " chỉ áp dụng cho đơn vé từ " + std::to_string(it->minTotal / 1000) + ".000 đ.");
        q.discount = std::min(q.ticketsTotal * it->percent / 100, it->maxOff);
        q.promoNote = it->desc;
    }
    q.total = q.ticketsTotal - q.discount + q.comboTotal;
}

Quote BoxOffice::quote(const OrderRequest& req, std::time_t now) const {
    Quote q;
    plan(req, now, q);
    return q;
}

Order BoxOffice::createOrder(const OrderRequest& req, std::time_t now) {
    string name = req.name;
    name.erase(0, name.find_first_not_of(' '));
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (name.empty()) throw BoxError("Nhập họ tên người nhận vé.");
    if (!req.owner.empty()) {
        int holding = 0;
        for (const auto& o : orders_) if (o.owner == req.owner && o.status == OrderStatus::Hold) ++holding;
        if (holding >= MAX_HOLD_ORDERS)
            throw BoxError("Bạn đang giữ " + std::to_string(holding) + " đơn chưa thanh toán. Hãy thanh toán hoặc hủy bớt trước khi đặt thêm.");
    }
    Quote q;
    plan(req, now, q);

    Show& sh = showOf(req.showtimeId);
    Order o;
    o.id = nextOrderId_;
    o.showtimeId = req.showtimeId;
    o.owner = req.owner;
    o.name = name;
    for (const auto& p : q.seats) {
        const auto r = sh.service.book(name, sh.seats.typeAt(p.row, p.col), req.owner, p);
        if (!r.seated) throw BoxError("Ghế vừa có người khác đặt, hãy chọn lại.");
        sh.service.findTicket(r.ticket.id)->orderId = o.id;
        o.ticketIds.push_back(r.ticket.id);
    }
    ++nextOrderId_;
    for (const auto& [id, qty] : req.combos) if (qty > 0) o.combos[id] = qty;
    o.promo = req.promo.empty() ? "" : upper(req.promo);
    o.ticketsTotal = q.ticketsTotal; o.comboTotal = q.comboTotal; o.discount = q.discount; o.total = q.total;
    o.status = OrderStatus::Hold;
    o.createdAt = now;
    o.holdUntil = now + HOLD_SECONDS;
    orders_.push_back(o);
    return o;
}

// ---------- thanh toán ----------
PayOutcome BoxOffice::pay(int orderId, const string& methodRaw, long cash, bool staff, std::time_t now) {
    Order* o = order(orderId);
    if (!o) throw BoxError("Không tìm thấy đơn hàng.", 404);
    if (o->status == OrderStatus::Paid) throw BoxError("Đơn hàng này đã được thanh toán.");
    if (o->status != OrderStatus::Hold) throw BoxError("Đơn hàng đã bị hủy hoặc hết hạn giữ ghế.");
    if (now > o->holdUntil) {
        releaseOrder(*o, OrderStatus::Expired, now, nullptr);
        throw BoxError("Đơn hàng đã hết thời gian giữ ghế, ghế đã được nhả.");
    }
    const string method = upper(methodRaw);
    PayOutcome out;
    if (method == "CASH") {
        if (!staff) throw BoxError("Thanh toán tiền mặt chỉ thực hiện tại quầy.", 403);
        if (cash < o->total) throw BoxError("Tiền khách đưa chưa đủ, còn thiếu " + std::to_string(o->total - cash) + " đ.");
        out.change = cash - o->total;
        o->payRef = "CASH";
    } else if (method == "BANK" || method == "CARD" || method == "WALLET") {
        char buf[32];
        std::snprintf(buf, sizeof buf, "TX%08ld", static_cast<long>((static_cast<long long>(now) * 7919 + o->id * 104729LL) % 100000000));
        o->payRef = buf;
    } else {
        throw BoxError("Phương thức thanh toán không hợp lệ.");
    }
    Show& sh = showOf(o->showtimeId);
    for (int id : o->ticketIds)
        if (Ticket* t = sh.service.findTicket(id)) t->status = TicketStatus::Paid;
    o->method = method;
    o->status = OrderStatus::Paid;
    o->paidAt = now;
    o->holdUntil = 0;
    out.order = *o;
    return out;
}

// ---------- hủy, hết hạn, hàng chờ ----------
void BoxOffice::adoptGranted(const std::vector<Ticket>& tickets, int showtimeId, std::time_t now, std::vector<Order>* out) {
    Show& sh = showOf(showtimeId);
    for (const auto& t : tickets) {
        Order n;
        n.id = nextOrderId_++;
        n.showtimeId = showtimeId;
        n.owner = t.owner;
        n.name = t.customerName;
        n.ticketIds = {t.id};
        n.ticketsTotal = n.total = t.price;
        n.status = OrderStatus::Hold;
        n.createdAt = now;
        n.holdUntil = now + HOLD_SECONDS;
        n.fromQueue = true;
        sh.service.findTicket(t.id)->orderId = n.id;
        orders_.push_back(n);
        if (out) out->push_back(n);
    }
}

// Lưu ý: hàm này có thể thêm đơn mới vào orders_, nên o không dùng được sau khi gọi.
void BoxOffice::releaseOrder(Order& o, OrderStatus finalStatus, std::time_t now, std::vector<Order>* granted) {
    Show& sh = showOf(o.showtimeId);
    const int showtimeId = o.showtimeId;
    std::vector<Ticket> newTickets;
    for (int id : o.ticketIds) {
        const Ticket* t = sh.service.findTicket(id);
        if (!t || t->status == TicketStatus::Cancelled) continue;
        auto res = sh.service.cancel(id);
        newTickets.insert(newTickets.end(), res.granted.begin(), res.granted.end());
    }
    o.status = finalStatus;
    o.holdUntil = 0;
    adoptGranted(newTickets, showtimeId, now, granted);
}

CancelOutcome BoxOffice::cancel(int orderId, bool staff, std::time_t now) {
    Order* o = order(orderId);
    if (!o) throw BoxError("Không tìm thấy đơn hàng.", 404);
    if (o->status == OrderStatus::Cancelled || o->status == OrderStatus::Expired) throw BoxError("Đơn hàng đã được hủy trước đó.");
    if (o->checkedInAt) throw BoxError("Vé đã được sử dụng, không thể hủy.");
    long refund = 0;
    if (o->status == OrderStatus::Paid) {
        const Showtime* st = findShowtime(o->showtimeId);
        if (!staff && st && now > st->start - CANCEL_BEFORE_MIN * 60)
            throw BoxError("Chỉ hủy được vé đã thanh toán trước giờ chiếu ít nhất " + std::to_string(CANCEL_BEFORE_MIN / 60) + " tiếng.");
        refund = o->total * REFUND_PERCENT / 100;
    }
    CancelOutcome out;
    const int id = o->id;
    o->refund = refund;
    releaseOrder(*o, OrderStatus::Cancelled, now, &out.granted);
    out.order = *order(id);
    out.refund = refund;
    return out;
}

int BoxOffice::sweep(std::time_t now) {
    int n = 0;
    for (std::size_t i = 0; i < orders_.size(); ++i) {
        if (orders_[i].status != OrderStatus::Hold || orders_[i].holdUntil > now) continue;
        Order copy = orders_[i];
        releaseOrder(orders_[i], OrderStatus::Expired, now, nullptr);
        (void)copy;
        ++n;
    }
    return n;
}

JoinOutcome BoxOffice::joinQueue(int showtimeId, SeatType type, const string& nameRaw, const string& owner, std::time_t now) {
    Show& sh = showOf(showtimeId);
    const Movie* mv = findMovie(sh.info.movieId);
    if (!mv || mv->status != "NOW") throw BoxError("Phim này chưa mở bán vé.");
    if (!salesOpen(sh.info, now)) throw BoxError("Suất chiếu đã ngừng bán vé.");
    string name = nameRaw;
    name.erase(0, name.find_first_not_of(' '));
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (name.empty()) throw BoxError("Nhập họ tên người nhận vé.");
    if (sh.seats.freeCount(type) > 0) throw BoxError("Vẫn còn ghế loại này, hãy đặt vé trực tiếp.");
    if (!owner.empty())
        for (const auto& c : sh.service.queueSnapshot())
            if (c.owner == owner) throw BoxError("Bạn đã đăng ký chờ ở suất chiếu này rồi.");
    const auto r = sh.service.book(name, type, owner);
    JoinOutcome out;
    if (r.seated) throw BoxError("Vẫn còn ghế loại này, hãy đặt vé trực tiếp.");
    out.position = r.queuePosition;
    const auto snap = sh.service.queueSnapshot();
    out.customer = snap[static_cast<std::size_t>(r.queuePosition - 1)];
    return out;
}

void BoxOffice::leaveQueue(int showtimeId, int customerId, const string& owner, bool staff) {
    Show& sh = showOf(showtimeId);
    for (const auto& c : sh.service.queueSnapshot()) {
        if (c.id != customerId) continue;
        if (!staff && (c.owner.empty() || c.owner != owner)) throw BoxError("Bạn chỉ rời hàng được với yêu cầu của chính mình.", 403);
        sh.service.leaveQueue(customerId);
        return;
    }
    throw BoxError("Không tìm thấy người này trong hàng chờ.", 404);
}

// ---------- soát vé ----------
PayOutcome BoxOffice::checkIn(const string& code, std::time_t now) {
    string seat;
    const int id = parseOrderCode(code, &seat);
    if (!id) throw BoxError("Mã vé không hợp lệ. Mã có dạng CQ000123.");
    Order* o = order(id);
    if (!o) throw BoxError("Không tìm thấy vé với mã này.", 404);
    if (o->status == OrderStatus::Hold) throw BoxError("Vé chưa thanh toán.");
    if (o->status != OrderStatus::Paid) throw BoxError("Vé đã bị hủy.");
    if (o->checkedInAt) {
        char buf[16];
        std::tm tmv = *std::localtime(&o->checkedInAt);
        std::strftime(buf, sizeof buf, "%H:%M", &tmv);
        throw BoxError(string("Vé đã được sử dụng lúc ") + buf + ".");
    }
    const Showtime* st = findShowtime(o->showtimeId);
    if (!st) throw BoxError("Không tìm thấy suất chiếu của vé.", 404);
    const Movie* mv = findMovie(st->movieId);
    if (now < st->start - CHECKIN_OPEN_MIN * 60) throw BoxError("Chưa đến giờ soát vé (mở trước giờ chiếu " + std::to_string(CHECKIN_OPEN_MIN) + " phút).");
    if (now > st->start + (mv ? mv->minutes : 120) * 60) throw BoxError("Suất chiếu đã kết thúc.");
    if (!seat.empty()) {
        bool found = false;
        for (const auto& t : ticketsOf(*o)) if (upper(SeatMap::seatName(t.row, t.col)) == seat) found = true;
        if (!found) throw BoxError("Ghế " + seat + " không thuộc đơn này.");
    }
    o->checkedInAt = now;
    PayOutcome out;
    out.order = *o;
    return out;
}

// ---------- thống kê ----------
Stats BoxOffice::stats() const {
    Stats s;
    std::map<int, MovieStat> byMovie;
    for (const auto& m : movies_) byMovie[m.id] = {m.id, m.title, 0, 0};
    for (const auto& o : orders_) {
        if (o.status == OrderStatus::Hold) ++s.ordersHold;
        if (o.status == OrderStatus::Cancelled) ++s.ordersCancelled;
        if (o.status == OrderStatus::Expired) ++s.ordersExpired;
        if (o.paidAt) {
            s.gross += o.total;
            s.refunds += o.refund;
            const Showtime* st = findShowtime(o.showtimeId);
            if (st && byMovie.count(st->movieId)) byMovie[st->movieId].revenue += o.total - o.refund;
        }
        if (o.status == OrderStatus::Paid) {
            ++s.ordersPaid;
            s.ticketsSold += static_cast<int>(o.ticketIds.size());
            if (o.checkedInAt) ++s.checkedIn;
            for (const auto& [id, qty] : o.combos) s.combos[id] += qty;
            const Showtime* st = findShowtime(o.showtimeId);
            if (st && byMovie.count(st->movieId)) byMovie[st->movieId].tickets += static_cast<int>(o.ticketIds.size());
        }
    }
    s.net = s.gross - s.refunds;
    for (auto& [id, m] : byMovie) s.movies.push_back(m);
    for (const auto& sh : shows_) {
        const int total = sh->seats.rows() * sh->seats.cols();
        const int freeN = sh->seats.freeCount(SeatType::Normal) + sh->seats.freeCount(SeatType::Vip);
        s.shows.push_back({sh->info.id, total - freeN, total});
        s.waiting += static_cast<int>(sh->service.queueSnapshot().size());
    }
    return s;
}

// ---------- lưu và nạp ----------
void BoxOffice::save() const {
    {
        std::ostringstream os;
        os << "id,title,genre,minutes,rating,status,synopsis\n";
        for (const auto& m : movies_)
            os << m.id << ',' << csvQuote(m.title) << ',' << csvQuote(m.genre) << ',' << m.minutes << ',' << m.rating << ',' << m.status << ',' << csvQuote(m.synopsis) << '\n';
        writeFileAtomic(path("movies.csv"), os.str());
    }
    {
        std::ostringstream os;
        os << "id,movieId,room,start,rows,cols,vipRows,priceNormal,priceVip\n";
        for (const auto& s : showtimes_)
            os << s.id << ',' << s.movieId << ',' << csvQuote(s.room) << ',' << static_cast<long long>(s.start) << ',' << s.rows << ',' << s.cols << ',' << s.vipRows << ',' << s.priceNormal << ',' << s.priceVip << '\n';
        writeFileAtomic(path("showtimes.csv"), os.str());
    }
    {
        std::ostringstream os;
        os << "id,showtimeId,owner,name,tickets,combos,promo,ticketsTotal,comboTotal,discount,total,status,method,payRef,createdAt,holdUntil,paidAt,checkedInAt,refund,fromQueue\n";
        for (const auto& o : orders_) {
            string tk, cb;
            for (int id : o.ticketIds) tk += (tk.empty() ? "" : ";") + std::to_string(id);
            for (const auto& [id, q] : o.combos) cb += (cb.empty() ? "" : ";") + id + ":" + std::to_string(q);
            os << o.id << ',' << o.showtimeId << ',' << csvQuote(o.owner) << ',' << csvQuote(o.name) << ',' << tk << ',' << cb << ',' << csvQuote(o.promo) << ','
               << o.ticketsTotal << ',' << o.comboTotal << ',' << o.discount << ',' << o.total << ',' << toString(o.status) << ',' << o.method << ',' << o.payRef << ','
               << static_cast<long long>(o.createdAt) << ',' << static_cast<long long>(o.holdUntil) << ',' << static_cast<long long>(o.paidAt) << ','
               << static_cast<long long>(o.checkedInAt) << ',' << o.refund << ',' << (o.fromQueue ? 1 : 0) << '\n';
        }
        writeFileAtomic(path("orders.csv"), os.str());
    }
    for (const auto& sh : shows_) {
        saveTickets(path("shows/tickets_" + std::to_string(sh->info.id) + ".csv"), sh->service.tickets());
        saveQueue(path("shows/queue_" + std::to_string(sh->info.id) + ".csv"), sh->service.queueSnapshot());
    }
}

void BoxOffice::rebuildShows() {
    shows_.clear();
    for (const auto& s : showtimes_) shows_.push_back(std::make_unique<Show>(s));
}

void BoxOffice::resetSales() {
    orders_.clear();
    nextOrderId_ = 1;
    rebuildShows();
}

void BoxOffice::load(std::time_t now) {
    if (!fileExists(path("movies.csv"))) {
        seed(now);
        save();
        return;
    }
    movies_.clear(); showtimes_.clear(); orders_.clear();
    auto lines = readLines(path("movies.csv"));
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 7) throw StorageError("movies.csv dong " + std::to_string(i + 1) + ": thieu cot");
        Movie m;
        m.id = static_cast<int>(toLong(f[0], "id")); m.title = f[1]; m.genre = f[2];
        m.minutes = static_cast<int>(toLong(f[3], "minutes")); m.rating = f[4]; m.status = f[5]; m.synopsis = f[6];
        movies_.push_back(m);
        nextMovieId_ = std::max(nextMovieId_, m.id + 1);
    }
    lines = readLines(path("showtimes.csv"));
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 9) throw StorageError("showtimes.csv dong " + std::to_string(i + 1) + ": thieu cot");
        Showtime s;
        s.id = static_cast<int>(toLong(f[0], "id")); s.movieId = static_cast<int>(toLong(f[1], "movieId")); s.room = f[2];
        s.start = static_cast<std::time_t>(toLong(f[3], "start"));
        s.rows = static_cast<int>(toLong(f[4], "rows")); s.cols = static_cast<int>(toLong(f[5], "cols")); s.vipRows = static_cast<int>(toLong(f[6], "vipRows"));
        s.priceNormal = toLong(f[7], "priceNormal"); s.priceVip = toLong(f[8], "priceVip");
        showtimes_.push_back(s);
        nextShowId_ = std::max(nextShowId_, s.id + 1);
    }

    // Tự động dời lịch chiếu về hiện tại nếu toàn bộ suất chiếu trong file đã qua thời gian hiện tại
    bool hasUpcoming = false;
    std::time_t minStart = 0;
    for (const auto& s : showtimes_) {
        if (s.start > now + 30 * 60) hasUpcoming = true;
        if (minStart == 0 || s.start < minStart) minStart = s.start;
    }
    bool shifted = false;
    if (!showtimes_.empty() && !hasUpcoming && minStart > 0) {
        std::tm tmNow = *std::localtime(&now);
        tmNow.tm_hour = 0; tmNow.tm_min = 0; tmNow.tm_sec = 0; tmNow.tm_isdst = -1;
        const std::time_t todayMidnight = std::mktime(&tmNow);

        std::tm tmFirst = *std::localtime(&minStart);
        tmFirst.tm_hour = 0; tmFirst.tm_min = 0; tmFirst.tm_sec = 0; tmFirst.tm_isdst = -1;
        const std::time_t firstMidnight = std::mktime(&tmFirst);

        std::time_t delta = todayMidnight - firstMidnight;
        for (auto& s : showtimes_) s.start += delta;

        // Nếu sau khi dời về hôm nay mà vẫn không còn suất nào mở bán (ví dụ chạy lúc tối muộn), dời sang ngày mai
        bool anyOpen = false;
        for (const auto& s : showtimes_) if (s.start > now + 30 * 60) anyOpen = true;
        if (!anyOpen) {
            for (auto& s : showtimes_) s.start += 86400;
        }
        shifted = true;
    }

    rebuildShows();
    for (auto& sh : shows_) {
        const string id = std::to_string(sh->info.id);
        sh->service.restore(loadTickets(path("shows/tickets_" + id + ".csv")), loadQueue(path("shows/queue_" + id + ".csv")));
    }
    lines = readLines(path("orders.csv"));
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 20) throw StorageError("orders.csv dong " + std::to_string(i + 1) + ": thieu cot");
        Order o;
        o.id = static_cast<int>(toLong(f[0], "id")); o.showtimeId = static_cast<int>(toLong(f[1], "showtimeId"));
        o.owner = f[2]; o.name = f[3];
        { std::stringstream ss(f[4]); string x; while (std::getline(ss, x, ';')) if (!x.empty()) o.ticketIds.push_back(static_cast<int>(toLong(x, "tickets"))); }
        { std::stringstream ss(f[5]); string x; while (std::getline(ss, x, ';')) { auto c = x.find(':'); if (c != string::npos) o.combos[x.substr(0, c)] = static_cast<int>(toLong(x.substr(c + 1), "combos")); } }
        o.promo = f[6];
        o.ticketsTotal = toLong(f[7], "ticketsTotal"); o.comboTotal = toLong(f[8], "comboTotal"); o.discount = toLong(f[9], "discount"); o.total = toLong(f[10], "total");
        o.status = orderStatusFromString(f[11]); o.method = f[12]; o.payRef = f[13];
        o.createdAt = static_cast<std::time_t>(toLong(f[14], "createdAt")); o.holdUntil = static_cast<std::time_t>(toLong(f[15], "holdUntil"));
        o.paidAt = static_cast<std::time_t>(toLong(f[16], "paidAt")); o.checkedInAt = static_cast<std::time_t>(toLong(f[17], "checkedInAt"));
        o.refund = toLong(f[18], "refund"); o.fromQueue = f[19] == "1";
        orders_.push_back(o);
        nextOrderId_ = std::max(nextOrderId_, o.id + 1);
    }
    if (shifted) save();
}

// ---------- dữ liệu mẫu ----------
void BoxOffice::seed(std::time_t now) {
    movies_.clear(); showtimes_.clear(); orders_.clear();
    nextMovieId_ = nextShowId_ = nextOrderId_ = 1;
    auto add = [&](const char* title, const char* genre, int min, const char* rating, const char* status, const char* syn) {
        Movie m; m.title = title; m.genre = genre; m.minutes = min; m.rating = rating; m.status = status; m.synopsis = syn;
        saveMovie(m);
    };
    add("Nữ hoàng nước mắt", "Tình cảm", 112, "T13", "NOW", "Câu chuyện tình yêu đầy nước mắt của một cặp vợ chồng đi qua những biến cố lớn của gia đình và hôn nhân.");
    add("Nhiệm vụ: Bất khả thi - Nghiệp báo cuối cùng", "Hành động", 128, "T16", "NOW", "Đặc vụ Ethan Hunt đối mặt nhiệm vụ nguy hiểm nhất đời mình để ngăn chặn một mối đe dọa toàn cầu.");
    add("Tuyển thủ Dê: “Mùi” vị chiến thắng", "Hoạt hình", 96, "P", "NOW", "Chú dê nhỏ đầy khát vọng bước vào đấu trường thể thao khốc liệt để chứng minh bản thân.");
    add("Ám ảnh", "Kinh dị", 104, "T18", "NOW", "Một gia đình chuyển đến ngôi nhà mới và dần bị những hiện tượng kỳ lạ đeo bám không dứt.");
    add("Hành Tinh Cuối Cùng", "Khoa học viễn tưởng", 139, "T13", "SOON", "Nhân loại gửi chuyến tàu cuối cùng đi tìm một ngôi nhà mới.");
    add("Tiệm Bánh Hạnh Phúc", "Gia đình", 101, "P", "SOON", "Tiệm bánh nhỏ đầu ngõ đứng trước nguy cơ đóng cửa và những người hàng xóm cùng ra tay.");

    struct Room { const char* name; int rows, cols, vip; };
    const Room rooms[3] = {{"Phòng 1", 5, 8, 1}, {"Phòng 2", 6, 10, 2}, {"Phòng 3", 4, 8, 1}};
    const int slotMin[5] = {10 * 60, 12 * 60 + 45, 15 * 60 + 30, 18 * 60 + 15, 21 * 60};
    std::tm base = *std::localtime(&now);
    base.tm_hour = 0; base.tm_min = 0; base.tm_sec = 0; base.tm_isdst = -1;
    for (int d = 0; d < 4; ++d) {
        for (int s = 0; s < 5; ++s) {
            for (int k = 0; k < 3; ++k) {
                std::tm t = base;
                t.tm_mday += d; t.tm_min = slotMin[s]; t.tm_isdst = -1;
                const std::time_t start = std::mktime(&t);
                const bool weekend = t.tm_wday == 0 || t.tm_wday == 6;
                const bool evening = slotMin[s] >= 17 * 60;
                Showtime sh;
                sh.movieId = (s * 3 + k + d) % 4 + 1;
                sh.room = rooms[k].name; sh.rows = rooms[k].rows; sh.cols = rooms[k].cols; sh.vipRows = rooms[k].vip;
                sh.start = start;
                sh.priceNormal = (evening ? 85000 : 70000) + (weekend ? 10000 : 0) + (k == 1 ? 10000 : 0);
                sh.priceVip = sh.priceNormal + 40000;
                if (start > now - 3 * 3600) saveShowtime(sh);  // bỏ các suất đã qua từ lâu
            }
        }
    }
    shows_.clear();
    rebuildShows();
    // Suất đầu tiên còn bán được gần kín chỗ, chừa 2 ghế thường và 2 ghế VIP giữa phòng để thử hàng chờ.
    for (const auto& st : showtimes_) {
        if (st.start < now + 40 * 60) continue;
        const Show& sh = showOf(st.id);
        const int normalRows = st.rows - st.vipRows;
        std::vector<Position> take;
        for (int r = 0; r < st.rows; ++r)
            for (int c = 0; c < st.cols; ++c) {
                const bool keep = (c == st.cols / 2 - 1 || c == st.cols / 2) && (r == normalRows / 2 || r == st.rows - 1);
                if (!keep) take.push_back({r, c});
            }
        (void)sh;
        for (std::size_t i = 0; i < take.size(); i += MAX_SEATS_PER_ORDER) {
            OrderRequest rq;
            rq.showtimeId = st.id; rq.name = "Khách tại quầy";
            for (std::size_t j = i; j < take.size() && j < i + MAX_SEATS_PER_ORDER; ++j) rq.seats.push_back(take[j]);
            const Order o = createOrder(rq, now);
            pay(o.id, "CASH", o.total, true, now);
        }
        break;
    }
}