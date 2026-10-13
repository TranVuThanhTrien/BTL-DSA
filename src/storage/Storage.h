// Đọc, ghi file dữ liệu. Người phụ trách: D
//
// Định dạng CSV, mỗi dòng một bản ghi, dòng đầu là tiêu đề:
//   data/tickets.csv : id,name,type,row,col,price,status,createdAt
//   data/queue.csv   : id,name,type            (đầu hàng ở dòng đầu tiên)
// type là NORMAL hoặc VIP; status là UNPAID, PAID hoặc CANCELLED.
// Tên khách có thể chứa dấu phẩy: phải bọc trong dấu nháy kép khi ghi và tách đúng khi đọc.
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "../core/Types.h"

class StorageError : public std::runtime_error {
public:
    explicit StorageError(const std::string& msg) : std::runtime_error(msg) {}
};

void saveTickets(const std::string& path, const std::vector<Ticket>& tickets);
std::vector<Ticket> loadTickets(const std::string& path);   // file chưa có thì trả về rỗng

void saveQueue(const std::string& path, const std::vector<Customer>& queue);
std::vector<Customer> loadQueue(const std::string& path);

// Chuyển đổi chuỗi <-> enum, dùng chung với server (A).
std::string toString(SeatType t);
std::string toString(TicketStatus s);
SeatType seatTypeFromString(const std::string& s);          // ném StorageError nếu lạ
TicketStatus statusFromString(const std::string& s);
