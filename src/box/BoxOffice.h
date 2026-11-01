// Phòng vé: phim, suất chiếu, đơn hàng, giữ ghế, thanh toán, hủy hoàn tiền, soát vé, hàng chờ.
// Mỗi suất chiếu có sơ đồ ghế và một BookingService riêng (hàng chờ Deque riêng).
// Mọi hàm nghiệp vụ nhận thời gian hiện tại `now` để kiểm thử được.
#pragma once
#include <ctime>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "../core/BookingService.h"
#include "../core/SeatMap.h"
#include "../core/Types.h"

constexpr int HOLD_SECONDS = 600;       // giữ ghế 10 phút chờ thanh toán
constexpr int MAX_SEATS_PER_ORDER = 8;
constexpr int MAX_HOLD_ORDERS = 2;      // mỗi tài khoản tối đa 2 đơn đang giữ ghế
constexpr int SALES_CLOSE_MIN = 10;     // ngừng bán trước giờ chiếu 10 phút
constexpr int CANCEL_BEFORE_MIN = 120;  // khách chỉ được hủy vé đã trả tiền trước giờ chiếu 2 tiếng
constexpr int CHECKIN_OPEN_MIN = 60;    // mở soát vé trước giờ chiếu 60 phút

class BoxError : public std::runtime_error {
public:
    explicit BoxError(const std::string& msg, int status = 400) : std::runtime_error(msg), status(status) {}
    int status;
};

struct Movie {
    int id = 0;
    std::string title, genre;
    int minutes = 90;
    std::string rating = "P";    // P, T13, T16, T18
    std::string status = "NOW";  // NOW: đang chiếu, SOON: sắp chiếu
    std::string synopsis;
};

struct Showtime {
    int id = 0;
    int movieId = 0;
    std::string room;
    std::time_t start = 0;
    int rows = 5, cols = 8, vipRows = 1;
    long priceNormal = 80000, priceVip = 120000;
};

struct Combo { std::string id, name; long price; };
struct Promo { std::string code; int percent; long maxOff; long minTotal; std::string desc; };

enum class OrderStatus { Hold, Paid, Cancelled, Expired };

struct Order {
    int id = 0;
    int showtimeId = 0;
    std::string owner;  // rỗng nếu bán tại quầy
    std::string name;
    std::vector<int> ticketIds;
    std::map<std::string, int> combos;
    std::string promo;
    long ticketsTotal = 0, comboTotal = 0, discount = 0, total = 0;
    OrderStatus status = OrderStatus::Hold;
    std::string method, payRef;
    std::time_t createdAt = 0, holdUntil = 0, paidAt = 0, checkedInAt = 0;
    long refund = 0;
    bool fromQueue = false;  // đơn tạo tự động khi khách trong hàng chờ được cấp ghế
};

struct OrderRequest {
    int showtimeId = 0;
    std::string name, owner;
    std::vector<Position> seats;            // chọn tay; để trống nếu dùng autoCount
    SeatType autoType = SeatType::Normal;
    int autoCount = 0;
    std::map<std::string, int> combos;
    std::string promo;
};

struct Quote {
    std::vector<Position> seats;
    SeatType type = SeatType::Normal;
    long ticketsTotal = 0, comboTotal = 0, discount = 0, total = 0;
    std::string promoNote;
};

struct PayOutcome { Order order; long change = 0; };
struct CancelOutcome { Order order; long refund = 0; std::vector<Order> granted; };
struct JoinOutcome { Customer customer; int position = 0; bool seated = false; Order order; };

struct MovieStat { int movieId = 0; std::string title; long revenue = 0; int tickets = 0; };
struct ShowStat { int showtimeId = 0; int taken = 0, total = 0; };
struct Stats {
    long gross = 0, refunds = 0, net = 0;
    int ticketsSold = 0, ordersPaid = 0, ordersCancelled = 0, ordersExpired = 0, ordersHold = 0, waiting = 0, checkedIn = 0;
    std::vector<MovieStat> movies;
    std::vector<ShowStat> shows;
    std::map<std::string, int> combos;
};

class BoxOffice {
public:
    explicit BoxOffice(std::string dir);

    // Nạp dữ liệu từ thư mục; chưa có dữ liệu thì tạo dữ liệu mẫu quanh thời điểm now.
    void load(std::time_t now);
    void save() const;
    void resetSales();  // xóa vé, đơn, hàng chờ; giữ phim và suất chiếu

    // ----- danh mục -----
    const std::vector<Movie>& movies() const { return movies_; }
    const std::vector<Showtime>& showtimes() const { return showtimes_; }
    const std::vector<Combo>& combos() const;
    const std::vector<Promo>& promos() const;
    const Movie* findMovie(int id) const;
    const Showtime* findShowtime(int id) const;
    int saveMovie(Movie m);                  // id == 0: thêm mới; ngược lại: sửa
    void removeMovie(int id);
    int saveShowtime(Showtime s);
    void removeShowtime(int id);

    // ----- ghế và hàng chờ -----
    const SeatMap& seatMap(int showtimeId) const;
    std::vector<Customer> queue(int showtimeId) const;
    bool salesOpen(const Showtime& s, std::time_t now) const;

    // ----- đơn hàng -----
    Quote quote(const OrderRequest& req, std::time_t now) const;
    Order createOrder(const OrderRequest& req, std::time_t now);
    PayOutcome pay(int orderId, const std::string& method, long cash, bool staff, std::time_t now);
    CancelOutcome cancel(int orderId, bool staff, std::time_t now);
    int sweep(std::time_t now);  // hủy các đơn hết hạn giữ ghế; trả về số đơn đã hủy
    JoinOutcome joinQueue(int showtimeId, SeatType type, const std::string& name, const std::string& owner, std::time_t now);
    void leaveQueue(int showtimeId, int customerId, const std::string& owner, bool staff);
    PayOutcome checkIn(const std::string& code, std::time_t now);

    const std::vector<Order>& orders() const { return orders_; }
    const Order* findOrder(int id) const;
    std::vector<Ticket> ticketsOf(const Order& o) const;
    Stats stats() const;

    static std::string orderCode(int id);          // 12 -> "CQ000012"
    static int parseOrderCode(const std::string&, std::string* seat = nullptr);  // 0 nếu sai định dạng

private:
    struct Show {
        Show(const Showtime& s) : info(s), seats(s.rows, s.cols, s.vipRows), service(seats) {
            service.setPrices(s.priceNormal, s.priceVip);
        }
        Showtime info;
        SeatMap seats;
        BookingService service;
    };
    Show& showOf(int showtimeId);
    const Show& showOf(int showtimeId) const;
    Order* order(int id);
    void plan(const OrderRequest& req, std::time_t now, Quote& q) const;
    void releaseOrder(Order& o, OrderStatus finalStatus, std::time_t now, std::vector<Order>* granted);
    void adoptGranted(const std::vector<Ticket>& tickets, int showtimeId, std::time_t now, std::vector<Order>* out);
    void seed(std::time_t now);
    void rebuildShows();
    std::string path(const std::string& file) const { return dir_ + "/" + file; }

    std::string dir_;
    std::vector<Movie> movies_;
    std::vector<Showtime> showtimes_;
    std::vector<Order> orders_;
    std::vector<std::unique_ptr<Show>> shows_;
    int nextOrderId_ = 1, nextMovieId_ = 1, nextShowId_ = 1;
};

std::string toString(OrderStatus s);
OrderStatus orderStatusFromString(const std::string& s);


