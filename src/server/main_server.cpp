// API server (cpp-httplib + nlohmann/json). Phần lõi (src/core, src/box) không biết gì về web,
// nên main_console vẫn chạy được như bản dự phòng khi demo.
//
// Mọi phản hồi là JSON. Lỗi: HTTP 4xx/5xx kèm { "error": "thông báo tiếng Việt" }.
// Đăng nhập trả về token; các yêu cầu cần đăng nhập gửi kèm header  X-Token: <token>.
//
//   Công khai: GET /api/catalog /api/shows?movie= /api/show?id= /api/news?cat= /api/me
//              POST /api/quote /api/register /api/login /api/logout
//   Đăng nhập: POST /api/orders /api/orders/pay /api/orders/cancel /api/queue/join /api/queue/leave ; GET /api/orders
//   Quản trị:  POST /api/checkin /api/movies/save|delete /api/showtimes/save|delete /api/news/create|update|delete /api/reset ; GET /api/stats
#include <algorithm>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>

#include "../../third_party/httplib.h"
#include "../../third_party/json.hpp"
#include "../box/BoxOffice.h"
#include "../storage/Storage.h"
#include "Auth.h"
#include "News.h"

using json = nlohmann::json;

static BoxOffice g_box("data");
static AuthService g_auth("data/users.csv");
static NewsService g_news("data/news.csv");
static std::mutex g_mutex;  // httplib xử lý nhiều yêu cầu cùng lúc, mọi truy cập dữ liệu phải khóa

// ---------- tiện ích ----------
static void sendJson(httplib::Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}
static void sendError(httplib::Response& res, int status, const std::string& msg) {
    sendJson(res, json{{"error", msg}}, status);
}
static std::optional<User> userOf(const httplib::Request& req) {
    const std::string token = req.get_header_value("X-Token");
    if (token.empty()) return std::nullopt;
    return g_auth.userByToken(token);
}
static json userJson(const User& u) {
    return json{{"username", u.username}, {"fullName", u.fullName}, {"role", u.role}};
}

// "Nguyễn Minh Anh" -> "Khách N.M.A." (lấy ký tự UTF-8 đầu của mỗi từ)
static std::string maskName(const std::string& name) {
    std::istringstream in(name);
    std::string word, out = "Khách ";
    while (in >> word) {
        std::size_t len = 1;
        while (len < word.size() && (static_cast<unsigned char>(word[len]) & 0xC0) == 0x80) ++len;
        out += word.substr(0, len) + ".";
    }
    return out;
}

static NewsInput newsInputFrom(const json& in) {
    NewsInput n;
    n.category = in.value("category", "");
    n.title = in.value("title", "");
    n.body = in.value("body", "");
    n.movie = in.value("movie", "");
    n.pinned = in.value("pinned", false);
    return n;
}

static json movieJson(const Movie& m) {
    int shows = 0;
    for (const auto& s : g_box.showtimes()) if (s.movieId == m.id) ++shows;
    return json{{"id", m.id}, {"title", m.title}, {"genre", m.genre}, {"minutes", m.minutes}, {"rating", m.rating},
                {"status", m.status}, {"synopsis", m.synopsis}, {"shows", shows}};
}

static json showJson(const Showtime& s, std::time_t now) {
    const SeatMap& sm = g_box.seatMap(s.id);
    const Movie* mv = g_box.findMovie(s.movieId);
    return json{{"id", s.id}, {"movieId", s.movieId}, {"movie", mv ? mv->title : ""}, {"room", s.room},
                {"start", static_cast<long long>(s.start)},
                {"end", static_cast<long long>(s.start + (mv ? mv->minutes : 120) * 60)},
                {"rows", s.rows}, {"cols", s.cols}, {"vipRows", s.vipRows},
                {"priceNormal", s.priceNormal}, {"priceVip", s.priceVip},
                {"freeNormal", sm.freeCount(SeatType::Normal)}, {"freeVip", sm.freeCount(SeatType::Vip)},
                {"waiting", g_box.queue(s.id).size()}, {"open", g_box.salesOpen(s, now) && mv && mv->status == "NOW"}};
}

static json orderJson(const Order& o, const std::optional<User>& viewer, std::time_t now) {
    const Showtime* st = g_box.findShowtime(o.showtimeId);
    const Movie* mv = st ? g_box.findMovie(st->movieId) : nullptr;
    json tickets = json::array();
    for (const auto& t : g_box.ticketsOf(o))
        tickets.push_back({{"seat", SeatMap::seatName(t.row, t.col)}, {"type", toString(t.type)}, {"price", t.price}});
    json combos = json::array();
    for (const auto& [id, qty] : o.combos)
        for (const auto& c : g_box.combos())
            if (c.id == id) combos.push_back({{"id", id}, {"name", c.name}, {"qty", qty}, {"price", c.price}});
    const bool admin = viewer && viewer->isAdmin();
    bool canCancel = false;
    if (o.status == OrderStatus::Hold) canCancel = true;
    else if (o.status == OrderStatus::Paid && !o.checkedInAt)
        canCancel = admin || (st && now <= st->start - CANCEL_BEFORE_MIN * 60);
    const int refundPreview = o.status == OrderStatus::Paid ? static_cast<int>(o.total * REFUND_PERCENT / 100) : 0;
    return json{{"id", o.id}, {"code", BoxOffice::orderCode(o.id)}, {"showId", o.showtimeId},
                {"movie", mv ? mv->title : ""}, {"room", st ? st->room : ""},
                {"start", st ? static_cast<long long>(st->start) : 0LL},
                {"name", o.name}, {"status", toString(o.status)}, {"tickets", tickets}, {"combos", combos},
                {"promo", o.promo}, {"ticketsTotal", o.ticketsTotal}, {"comboTotal", o.comboTotal},
                {"discount", o.discount}, {"total", o.total}, {"method", o.method}, {"payRef", o.payRef},
                {"createdAt", static_cast<long long>(o.createdAt)}, {"holdUntil", static_cast<long long>(o.holdUntil)},
                {"paidAt", static_cast<long long>(o.paidAt)}, {"checkedInAt", static_cast<long long>(o.checkedInAt)},
                {"refund", o.refund}, {"refundPreview", refundPreview}, {"fromQueue", o.fromQueue},
                {"owner", admin ? o.owner : std::string()}, {"canCancel", canCancel},
                {"mine", viewer && !o.owner.empty() && o.owner == viewer->username}};
}

static OrderRequest requestFrom(const json& in, const std::string& owner) {
    OrderRequest rq;
    rq.showtimeId = in.value("showId", 0);
    rq.name = in.value("name", "");
    rq.owner = owner;
    rq.promo = in.value("promo", "");
    if (in.contains("seats") && in["seats"].is_array())
        for (const auto& s : in["seats"]) rq.seats.push_back({s.value("row", -1), s.value("col", -1)});
    rq.autoCount = in.value("count", 0);
    rq.autoType = in.value("type", "NORMAL") == "VIP" ? SeatType::Vip : SeatType::Normal;
    if (in.contains("combos") && in["combos"].is_object())
        for (auto it = in["combos"].begin(); it != in["combos"].end(); ++it)
            if (it.value().is_number_integer()) rq.combos[it.key()] = it.value().get<int>();
    return rq;
}

// Chạy một xử lý đã khóa mutex, nhả các đơn hết hạn trước, và đổi mọi ngoại lệ thành phản hồi lỗi JSON.
template <typename F>
static void guarded(httplib::Response& res, bool writes, F fn) {
    std::lock_guard<std::mutex> lock(g_mutex);
    try {
        const int expired = g_box.sweep(std::time(nullptr));
        fn();
        if (writes || expired > 0) g_box.save();
    } catch (const BoxError& e) {
        sendError(res, e.status, e.what());
    } catch (const json::exception&) {
        sendError(res, 400, "Dữ liệu gửi lên không hợp lệ.");
    } catch (const std::invalid_argument& e) {
        sendError(res, 400, e.what());
    } catch (const StorageError& e) {
        sendError(res, 500, std::string("Lỗi lưu dữ liệu: ") + e.what());
    } catch (const std::exception& e) {
        sendError(res, 500, e.what());
    }
}

static std::optional<User> requireUser(const httplib::Request& req, httplib::Response& res) {
    auto u = userOf(req);
    if (!u) sendError(res, 401, "Bạn cần đăng nhập để thực hiện thao tác này.");
    return u;
}
static std::optional<User> requireAdmin(const httplib::Request& req, httplib::Response& res) {
    auto u = requireUser(req, res);
    if (u && !u->isAdmin()) { sendError(res, 403, "Chức năng này chỉ dành cho quản trị viên."); return std::nullopt; }
    return u;
}

int main(int argc, char** argv) {
    const int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    try {
        g_box.load(std::time(nullptr));
    } catch (const std::exception& e) {
        std::cerr << "Khong nap duoc du lieu trong thu muc data/: " << e.what()
                  << "\nSua hoac xoa file loi roi chay lai.\n";
        return 1;
    }

    httplib::Server svr;
    using Req = const httplib::Request&;
    using Res = httplib::Response&;

    // ---------- công khai ----------
    svr.Get("/api/catalog", [](Req, Res res) {
        guarded(res, false, [&] {
            json movies = json::array(), combos = json::array(), promos = json::array();
            for (const auto& m : g_box.movies()) movies.push_back(movieJson(m));
            for (const auto& c : g_box.combos()) combos.push_back({{"id", c.id}, {"name", c.name}, {"price", c.price}});
            for (const auto& p : g_box.promos()) promos.push_back({{"code", p.code}, {"desc", p.desc}});
            sendJson(res, {{"movies", movies}, {"combos", combos}, {"promos", promos},
                           {"rules", {{"holdSeconds", HOLD_SECONDS}, {"maxSeats", MAX_SEATS_PER_ORDER},
                                      {"cancelBeforeMin", CANCEL_BEFORE_MIN}, {"refundPercent", REFUND_PERCENT},
                                      {"salesCloseMin", SALES_CLOSE_MIN}, {"checkinOpenMin", CHECKIN_OPEN_MIN}}},
                           {"now", static_cast<long long>(std::time(nullptr))}});
        });
    });
    svr.Get("/api/shows", [](Req req, Res res) {
        guarded(res, false, [&] {
            const int movie = req.has_param("movie") ? std::atoi(req.get_param_value("movie").c_str()) : 0;
            const std::time_t now = std::time(nullptr);
            auto list = g_box.showtimes();
            std::sort(list.begin(), list.end(), [](const Showtime& a, const Showtime& b) { return a.start < b.start; });
            json arr = json::array();
            for (const auto& s : list) {
                if (movie && s.movieId != movie) continue;
                if (s.start + 30 * 60 < now) continue;  // suất đã chiếu quá nửa tiếng thì ẩn
                arr.push_back(showJson(s, now));
            }
            sendJson(res, {{"items", arr}});
        });
    });
    svr.Get("/api/show", [](Req req, Res res) {
        guarded(res, false, [&] {
            const int id = std::atoi(req.get_param_value("id").c_str());
            const Showtime* s = g_box.findShowtime(id);
            if (!s) { sendError(res, 404, "Không tìm thấy suất chiếu."); return; }
            const SeatMap& sm = g_box.seatMap(id);
            json rows = json::array();
            for (int r = 0; r < sm.rows(); ++r) {
                json row = json::array();
                for (int c = 0; c < sm.cols(); ++c) row.push_back({{"type", toString(sm.typeAt(r, c))}, {"taken", !sm.isFree(r, c)}});
                rows.push_back(row);
            }
            const auto u = userOf(req);
            json q = json::array();
            for (const auto& c : g_box.queue(id)) {
                const bool mine = u && !c.owner.empty() && c.owner == u->username;
                const bool full = mine || (u && u->isAdmin());
                q.push_back({{"id", c.id}, {"name", full ? c.name : maskName(c.name)}, {"type", toString(c.type)}, {"mine", mine}, {"canLeave", full}});
            }
            sendJson(res, {{"show", showJson(*s, std::time(nullptr))}, {"seats", rows}, {"queue", q}});
        });
    });
    svr.Post("/api/quote", [](Req req, Res res) {
        guarded(res, false, [&] {
            const Quote q = g_box.quote(requestFrom(json::parse(req.body), ""), std::time(nullptr));
            json seats = json::array();
            for (const auto& p : q.seats) seats.push_back({{"row", p.row}, {"col", p.col}, {"seat", SeatMap::seatName(p.row, p.col)}});
            sendJson(res, {{"seats", seats}, {"ticketsTotal", q.ticketsTotal}, {"comboTotal", q.comboTotal},
                           {"discount", q.discount}, {"total", q.total}, {"promoNote", q.promoNote}});
        });
    });
    svr.Get("/api/news", [](Req req, Res res) {
        guarded(res, false, [&] {
            json arr = json::array();
            for (const auto& n : g_news.list(req.has_param("cat") ? req.get_param_value("cat") : ""))
                arr.push_back({{"id", n.id}, {"category", n.category}, {"title", n.title}, {"body", n.body},
                               {"movie", n.movie}, {"pinned", n.pinned}, {"ts", n.ts}, {"author", n.author}});
            sendJson(res, {{"items", arr}, {"categories", newsCategories()}});
        });
    });

    // ---------- tài khoản ----------
    svr.Post("/api/register", [](Req req, Res res) {
        guarded(res, false, [&] {
            const json in = json::parse(req.body);
            const auto r = g_auth.registerUser(in.value("fullName", ""), in.value("username", ""), in.value("password", ""), in.value("confirm", ""));
            if (!r.ok) { sendError(res, 400, r.error); return; }
            sendJson(res, {{"token", r.token}, {"user", userJson(r.user)}});
        });
    });
    svr.Post("/api/login", [](Req req, Res res) {
        guarded(res, false, [&] {
            const json in = json::parse(req.body);
            const auto r = g_auth.login(in.value("username", ""), in.value("password", ""));
            if (!r.ok) { sendError(res, 401, r.error); return; }
            sendJson(res, {{"token", r.token}, {"user", userJson(r.user)}});
        });
    });
    svr.Post("/api/logout", [](Req req, Res res) {
        guarded(res, false, [&] {
            g_auth.logout(req.get_header_value("X-Token"));
            sendJson(res, {{"ok", true}});
        });
    });
    svr.Get("/api/me", [](Req req, Res res) {
        guarded(res, false, [&] {
            const auto u = userOf(req);
            sendJson(res, {{"user", u ? userJson(*u) : json(nullptr)}});
        });
    });

    // ---------- đơn hàng (cần đăng nhập) ----------
    svr.Post("/api/orders", [](Req req, Res res) {
        guarded(res, true, [&] {
            const auto u = requireUser(req, res);
            if (!u) return;
            const std::time_t now = std::time(nullptr);
            const Order o = g_box.createOrder(requestFrom(json::parse(req.body), u->isAdmin() ? "" : u->username), now);
            sendJson(res, {{"order", orderJson(o, u, now)}});
        });
    });
    svr.Get("/api/orders", [](Req req, Res res) {
        guarded(res, false, [&] {
            const auto u = requireUser(req, res);
            if (!u) return;
            const std::time_t now = std::time(nullptr);
            json arr = json::array();
            std::vector<const Order*> mine;
            for (const auto& o : g_box.orders())
                if (u->isAdmin() || (!o.owner.empty() && o.owner == u->username)) mine.push_back(&o);
            std::sort(mine.begin(), mine.end(), [](const Order* a, const Order* b) { return a->id > b->id; });
            for (const Order* o : mine) arr.push_back(orderJson(*o, u, now));
            sendJson(res, {{"items", arr}});
        });
    });
    svr.Post("/api/orders/pay", [](Req req, Res res) {
        guarded(res, true, [&] {
            const auto u = requireUser(req, res);
            if (!u) return;
            const json in = json::parse(req.body);
            const Order* o = g_box.findOrder(in.value("orderId", 0));
            if (!o) { sendError(res, 404, "Không tìm thấy đơn hàng."); return; }
            if (!u->isAdmin() && (o->owner.empty() || o->owner != u->username)) { sendError(res, 403, "Bạn chỉ thanh toán được đơn của chính mình."); return; }
            const std::time_t now = std::time(nullptr);
            const auto p = g_box.pay(o->id, in.value("method", ""), in.value("cash", 0L), u->isAdmin(), now);
            sendJson(res, {{"order", orderJson(p.order, u, now)}, {"change", p.change}});
        });
    });
    svr.Post("/api/orders/cancel", [](Req req, Res res) {
        guarded(res, true, [&] {
            const auto u = requireUser(req, res);
            if (!u) return;
            const Order* o = g_box.findOrder(json::parse(req.body).value("orderId", 0));
            if (!o) { sendError(res, 404, "Không tìm thấy đơn hàng."); return; }
            if (!u->isAdmin() && (o->owner.empty() || o->owner != u->username)) { sendError(res, 403, "Bạn chỉ hủy được đơn của chính mình."); return; }
            const std::time_t now = std::time(nullptr);
            const auto c = g_box.cancel(o->id, u->isAdmin(), now);
            json granted = json::array();
            for (const auto& g : c.granted) granted.push_back({{"name", g.name}});
            sendJson(res, {{"order", orderJson(c.order, u, now)}, {"refund", c.refund}, {"granted", granted}});
        });
    });
    svr.Post("/api/queue/join", [](Req req, Res res) {
        guarded(res, true, [&] {
            const auto u = requireUser(req, res);
            if (!u) return;
            const json in = json::parse(req.body);
            const auto j = g_box.joinQueue(in.value("showId", 0), in.value("type", "NORMAL") == "VIP" ? SeatType::Vip : SeatType::Normal,
                                           in.value("name", ""), u->isAdmin() ? "" : u->username, std::time(nullptr));
            sendJson(res, {{"position", j.position}});
        });
    });
    svr.Post("/api/queue/leave", [](Req req, Res res) {
        guarded(res, true, [&] {
            const auto u = requireUser(req, res);
            if (!u) return;
            const json in = json::parse(req.body);
            g_box.leaveQueue(in.value("showId", 0), in.value("customerId", 0), u->username, u->isAdmin());
            sendJson(res, {{"ok", true}});
        });
    });

    // ---------- quản trị ----------
    svr.Post("/api/checkin", [](Req req, Res res) {
        guarded(res, true, [&] {
            const auto u = requireAdmin(req, res);
            if (!u) return;
            const std::time_t now = std::time(nullptr);
            const auto p = g_box.checkIn(json::parse(req.body).value("code", ""), now);
            sendJson(res, {{"order", orderJson(p.order, u, now)}});
        });
    });
    svr.Get("/api/stats", [](Req req, Res res) {
        guarded(res, false, [&] {
            if (!requireAdmin(req, res)) return;
            const Stats s = g_box.stats();
            json movies = json::array(), shows = json::array(), combos = json::array();
            for (const auto& m : s.movies) movies.push_back({{"id", m.movieId}, {"title", m.title}, {"revenue", m.revenue}, {"tickets", m.tickets}});
            for (const auto& x : s.shows) shows.push_back({{"id", x.showtimeId}, {"taken", x.taken}, {"total", x.total}});
            for (const auto& [id, q] : s.combos)
                for (const auto& c : g_box.combos()) if (c.id == id) combos.push_back({{"name", c.name}, {"qty", q}, {"revenue", c.price * q}});
            sendJson(res, {{"gross", s.gross}, {"refunds", s.refunds}, {"net", s.net}, {"ticketsSold", s.ticketsSold},
                           {"ordersPaid", s.ordersPaid}, {"ordersCancelled", s.ordersCancelled}, {"ordersExpired", s.ordersExpired},
                           {"ordersHold", s.ordersHold}, {"waiting", s.waiting}, {"checkedIn", s.checkedIn},
                           {"movies", movies}, {"shows", shows}, {"combos", combos}});
        });
    });
    svr.Post("/api/movies/save", [](Req req, Res res) {
        guarded(res, true, [&] {
            if (!requireAdmin(req, res)) return;
            const json in = json::parse(req.body);
            Movie m;
            m.id = in.value("id", 0); m.title = in.value("title", ""); m.genre = in.value("genre", "");
            m.minutes = in.value("minutes", 0); m.rating = in.value("rating", "P"); m.status = in.value("status", "NOW");
            m.synopsis = in.value("synopsis", "");
            sendJson(res, {{"id", g_box.saveMovie(m)}});
        });
    });
    svr.Post("/api/movies/delete", [](Req req, Res res) {
        guarded(res, true, [&] {
            if (!requireAdmin(req, res)) return;
            g_box.removeMovie(json::parse(req.body).value("id", 0));
            sendJson(res, {{"ok", true}});
        });
    });
    svr.Post("/api/showtimes/save", [](Req req, Res res) {
        guarded(res, true, [&] {
            if (!requireAdmin(req, res)) return;
            const json in = json::parse(req.body);
            Showtime s;
            s.id = in.value("id", 0); s.movieId = in.value("movieId", 0); s.room = in.value("room", "");
            s.start = static_cast<std::time_t>(in.value("start", 0LL));
            s.rows = in.value("rows", 5); s.cols = in.value("cols", 8); s.vipRows = in.value("vipRows", 1);
            s.priceNormal = in.value("priceNormal", 0L); s.priceVip = in.value("priceVip", 0L);
            sendJson(res, {{"id", g_box.saveShowtime(s)}});
        });
    });
    svr.Post("/api/showtimes/delete", [](Req req, Res res) {
        guarded(res, true, [&] {
            if (!requireAdmin(req, res)) return;
            g_box.removeShowtime(json::parse(req.body).value("id", 0));
            sendJson(res, {{"ok", true}});
        });
    });
    svr.Post("/api/news/create", [](Req req, Res res) {
        guarded(res, false, [&] {
            const auto u = requireAdmin(req, res);
            if (!u) return;
            sendJson(res, {{"id", g_news.create(newsInputFrom(json::parse(req.body)), u->fullName).id}});
        });
    });
    svr.Post("/api/news/update", [](Req req, Res res) {
        guarded(res, false, [&] {
            if (!requireAdmin(req, res)) return;
            const json in = json::parse(req.body);
            sendJson(res, {{"id", g_news.update(in.value("id", 0), newsInputFrom(in)).id}});
        });
    });
    svr.Post("/api/news/delete", [](Req req, Res res) {
        guarded(res, false, [&] {
            if (!requireAdmin(req, res)) return;
            g_news.remove(json::parse(req.body).value("id", 0));
            sendJson(res, {{"ok", true}});
        });
    });
    svr.Post("/api/reset", [](Req req, Res res) {
        guarded(res, true, [&] {
            if (!requireAdmin(req, res)) return;
            g_box.resetSales();
            sendJson(res, {{"ok", true}});
        });
    });

    svr.set_mount_point("/", "./web");  // phục vụ giao diện web

    std::cout << "Mo trinh duyet tai http://localhost:" << port << "\n"
              << "Tai khoan thu: admin / admin123 va khach / khach123\n";
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Khong mo duoc cong " << port << " (co the dang bi chuong trinh khac dung)\n";
        return 1;
    }
    return 0;
}


