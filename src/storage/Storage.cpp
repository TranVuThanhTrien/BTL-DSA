#include "Storage.h"

#include <fstream>

std::string toString(SeatType t) { return t == SeatType::Vip ? "VIP" : "NORMAL"; }

std::string toString(TicketStatus s) {
    switch (s) {
        case TicketStatus::Paid: return "PAID";
        case TicketStatus::Cancelled: return "CANCELLED";
        default: return "UNPAID";
    }
}

SeatType seatTypeFromString(const std::string& s) {
    if (s == "VIP") return SeatType::Vip;
    if (s == "NORMAL") return SeatType::Normal;
    throw StorageError("Loai ghe khong hop le: " + s);
}

TicketStatus statusFromString(const std::string& s) {
    // TODO: UNPAID / PAID / CANCELLED, còn lại ném StorageError
    (void)s;
    throw StorageError("TODO: statusFromString");
}

void saveTickets(const std::string& path, const std::vector<Ticket>& tickets) {
    // TODO: mở file ghi (lỗi mở thì ném StorageError), ghi dòng tiêu đề rồi từng vé.
    //       Ghi ra file tạm rồi đổi tên để mất điện giữa chừng không làm hỏng dữ liệu cũ.
    (void)path; (void)tickets;
    throw StorageError("TODO: saveTickets");
}

std::vector<Ticket> loadTickets(const std::string& path) {
    // TODO: file chưa tồn tại thì trả về rỗng. Dòng sai định dạng thì ném StorageError
    //       kèm số dòng để người dùng sửa được.
    (void)path;
    throw StorageError("TODO: loadTickets");
}

void saveQueue(const std::string& path, const std::vector<Customer>& queue) {
    // TODO
    (void)path; (void)queue;
    throw StorageError("TODO: saveQueue");
}

std::vector<Customer> loadQueue(const std::string& path) {
    // TODO
    (void)path;
    throw StorageError("TODO: loadQueue");
}
