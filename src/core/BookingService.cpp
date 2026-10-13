#include "BookingService.h"

#include <ctime>
#include <stdexcept>

BookingService::BookingService(SeatMap& seats) : seats_(seats) {}

Ticket BookingService::makeTicket(const std::string& name, SeatType type, Position pos) {
    Ticket t;
    t.id = nextTicketId_++;
    t.customerName = name;
    t.type = type;
    t.row = pos.row;
    t.col = pos.col;
    t.price = priceOf(type);
    t.status = TicketStatus::Unpaid;
    t.createdAt = std::time(nullptr);
    return t;
}

BookResult BookingService::book(const std::string& name, SeatType type) {
    // TODO: 1. findFree(type); nếu có thì occupy, makeTicket, lưu vào tickets_, trả seated = true.
    //       2. nếu hết: tạo Customer, VIP -> waiting_.pushFront, thường -> waiting_.pushBack,
    //          trả seated = false và queuePosition (VIP luôn là 1 nếu hàng chỉ có VIP mới vào).
    (void)name; (void)type;
    throw std::logic_error("TODO: book");
}

CancelResult BookingService::cancel(int ticketId) {
    // TODO: 1. findTicket; không có hoặc đã Cancelled thì ném std::invalid_argument.
    //       2. refund = (Paid ? price * REFUND_PERCENT / 100 : 0); đổi trạng thái Cancelled.
    //       3. seats_.release(row, col).
    //       4. granted = serveQueue().
    (void)ticketId;
    throw std::logic_error("TODO: cancel");
}

std::vector<Ticket> BookingService::serveQueue() {
    // TODO: lặp đến khi không cấp được nữa:
    //   - quét hàng từ đầu, tìm khách đầu tiên mà seats_.findFree(loại của khách) có ghế
    //     (waiting_.removeFirstIf với điều kiện đó, nhận khách vào biến removed)
    //   - occupy ghế, makeTicket, lưu vào tickets_ và vào danh sách trả về
    return {};
}

Ticket* BookingService::findTicket(int id) {
    for (auto& t : tickets_)
        if (t.id == id) return &t;
    return nullptr;
}

std::vector<Customer> BookingService::queueSnapshot() const {
    return waiting_.toVector();
}

void BookingService::restore(std::vector<Ticket> tickets, std::vector<Customer> queue) {
    // TODO: thay tickets_, nạp lại waiting_ (pushBack lần lượt), đặt lại nextTicketId_ và
    //       nextCustomerId_ lớn hơn id lớn nhất, occupy lại ghế của các vé chưa hủy.
    (void)tickets; (void)queue;
}
