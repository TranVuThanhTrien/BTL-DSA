// Thanh toán và xuất vé. Người phụ trách: D
#pragma once
#include <string>

#include "../core/Types.h"

struct PaymentResult {
    bool ok = false;
    long change = 0;          // tiền thừa trả khách
    std::string message;      // lý do khi ok == false (hiện thẳng cho người dùng)
};

// Thanh toán một vé. Chỉ vé Unpaid mới thanh toán được; cash phải >= giá vé.
// Thành công thì đổi status thành Paid. Không ném lỗi cho các lỗi nghiệp vụ thường gặp.
PaymentResult payTicket(Ticket& ticket, long cash);

// Nội dung vé dạng văn bản (để in ra màn hình hoặc ghi file .txt).
std::string ticketText(const Ticket& ticket, const std::string& movieTitle);

// Ghi vé ra file. Ném StorageError nếu không ghi được.
void writeTicketFile(const std::string& path, const Ticket& ticket, const std::string& movieTitle);
