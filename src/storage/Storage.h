// Đọc, ghi file dữ liệu dạng CSV, mỗi dòng một bản ghi, dòng đầu là tiêu đề.
//   data/tickets.csv : id,name,type,row,col,price,status,createdAt,owner,refund,orderId
//   data/queue.csv   : id,name,type,owner            (đầu hàng ở dòng đầu tiên)
// type là NORMAL hoặc VIP; status là UNPAID, PAID hoặc CANCELLED.
// Trường có dấu phẩy, nháy kép hoặc xuống dòng được bọc trong nháy kép khi ghi.
// Ghi file theo kiểu "ghi ra file tạm rồi đổi tên" để mất điện giữa chừng không hỏng dữ liệu cũ.
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
std::vector<Customer> loadQueue(const std::string& path);   // file chưa có thì trả về rỗng

bool fileExists(const std::string& path);

// Chuyển đổi chuỗi <-> enum.
std::string toString(SeatType t);
std::string toString(TicketStatus s);
SeatType seatTypeFromString(const std::string& s);          // ném StorageError nếu lạ
TicketStatus statusFromString(const std::string& s);

// Tiện ích CSV dùng chung với các module khác (tài khoản, tin tức).
std::string csvQuote(const std::string& field);
std::vector<std::string> csvSplit(const std::string& line);
void writeFileAtomic(const std::string& path, const std::string& content);
std::vector<std::string> readLines(const std::string& path, bool* existed = nullptr);
