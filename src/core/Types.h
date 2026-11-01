// Kiểu dữ liệu và hằng số dùng chung toàn dự án.
#pragma once
#include <ctime>
#include <string>

enum class SeatType { Normal, Vip };
enum class TicketStatus { Unpaid, Paid, Cancelled };

constexpr long PRICE_NORMAL = 80000;
constexpr long PRICE_VIP = 120000;
constexpr int REFUND_PERCENT = 80;  // hủy vé đã thanh toán được hoàn 80%

// Phòng mặc định khi dùng riêng BookingService: 5 hàng x 8 cột, hàng cuối là ghế VIP.
constexpr int ROOM_ROWS = 5;
constexpr int ROOM_COLS = 8;
constexpr int ROOM_VIP_ROWS = 1;

struct Position {
    int row;
    int col;
};

// Khách đang đứng trong hàng chờ. type == Vip nghĩa là khách ưu tiên.
struct Customer {
    int id = 0;
    std::string name;
    SeatType type = SeatType::Normal;
    std::string owner;  // tên đăng nhập của người đặt, rỗng nếu đặt tại quầy
};

struct Ticket {
    int id = 0;
    std::string customerName;
    SeatType type = SeatType::Normal;
    int row = 0;
    int col = 0;
    long price = 0;
    TicketStatus status = TicketStatus::Unpaid;
    std::time_t createdAt = 0;
    std::string owner;  // tên đăng nhập của người đặt, rỗng nếu đặt tại quầy
    long refund = 0;    // số tiền đã hoàn khi hủy vé đã thanh toán
    int orderId = 0;    // đơn hàng chứa vé này (0 nếu đặt bằng BookingService trực tiếp)
};

inline long priceOf(SeatType t) { return t == SeatType::Vip ? PRICE_VIP : PRICE_NORMAL; }


