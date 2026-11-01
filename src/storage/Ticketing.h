// Thanh toán và xuất vé.
#pragma once
#include <string>

#include "../core/Types.h"

struct PaymentResult {
    bool ok = false;
    long change = 0;          // tiền thừa trả khách
    std::string message;      // lý do khi ok == false (hiện thẳng cho người dùng)
};

std::string formatMoney(long amount);   // 80000 -> "80.000 đ"
std::string ticketCode(int id);         // 12 -> "VE0012"

// Thanh toán một vé. Chỉ vé Unpaid mới thanh toán được; cash phải >= giá vé.
// Thành công thì đổi status thành Paid. Các lỗi nghiệp vụ thường gặp trả về trong message, không ném lỗi.
PaymentResult payTicket(Ticket& ticket, long cash);

// Nội dung vé dạng văn bản (để in hoặc ghi file .txt).
std::string ticketText(const Ticket& ticket, const std::string& movieTitle, const std::string& showTime,
                       const std::string& room);

// Ghi vé ra file. Ném StorageError nếu không ghi được.
void writeTicketFile(const std::string& path, const Ticket& ticket, const std::string& movieTitle,
                     const std::string& showTime, const std::string& room);
