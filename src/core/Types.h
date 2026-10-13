// Kiểu dữ liệu dùng chung. Người phụ trách: A (hợp đồng chung, đổi thì báo cả nhóm).
#pragma once
#include <ctime>
#include <string>

enum class SeatType { Normal, Vip };
enum class TicketStatus { Unpaid, Paid, Cancelled };

constexpr long PRICE_NORMAL = 80000;
constexpr long PRICE_VIP = 120000;
constexpr int REFUND_PERCENT = 80;  // hủy vé đã thanh toán được hoàn 80%

struct Position {
    int row;
    int col;
};

// Khách đang đứng trong hàng chờ. type == Vip nghĩa là khách ưu tiên.
struct Customer {
    int id;
    std::string name;
    SeatType type;
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
};

inline long priceOf(SeatType t) { return t == SeatType::Vip ? PRICE_VIP : PRICE_NORMAL; }
