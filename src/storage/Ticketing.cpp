#include "Ticketing.h"

#include "../core/SeatMap.h"
#include "Storage.h"

PaymentResult payTicket(Ticket& ticket, long cash) {
    // TODO: kiểm tra trạng thái và số tiền, trả PaymentResult với message tiếng Việt rõ ràng
    //       (ví dụ "Còn thiếu 20.000 đ"). Đổi ticket.status = Paid khi thành công.
    (void)ticket; (void)cash;
    PaymentResult r;
    r.message = "TODO: payTicket";
    return r;
}

std::string ticketText(const Ticket& ticket, const std::string& movieTitle) {
    // TODO: dùng SeatMap::seatName(ticket.row, ticket.col) để in tên ghế
    (void)ticket; (void)movieTitle;
    return "";
}

void writeTicketFile(const std::string& path, const Ticket& ticket, const std::string& movieTitle) {
    // TODO: ghi ticketText ra file, lỗi mở file thì ném StorageError
    (void)path; (void)ticket; (void)movieTitle;
    throw StorageError("TODO: writeTicketFile");
}
