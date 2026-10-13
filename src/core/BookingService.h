// Điều phối vé ưu tiên/thường, hủy vé, hàng chờ. Người phụ trách: C
// Phụ thuộc: SeatMap (B), Deque (A).
//
// Quy tắc nghiệp vụ (chốt với giảng viên trước khi viết Chương 1):
//   - Còn ghế đúng loại: tạo vé trạng thái Unpaid, giữ ghế.
//   - Hết ghế: khách VIP vào ĐẦU hàng chờ (pushFront), khách thường vào CUỐI (pushBack).
//   - Hủy vé: giải phóng ghế, rồi cấp ghế đó cho khách ĐẦU hàng phù hợp
//     (popFront; nếu người đầu hàng cần loại ghế khác thì quét tiếp, gỡ giữa chuỗi bằng removeFirstIf).
//   - Hủy vé đã thanh toán hoàn REFUND_PERCENT, vé chưa thanh toán không hoàn.
#pragma once
#include <string>
#include <vector>

#include "Deque.h"
#include "SeatMap.h"
#include "Types.h"

struct BookResult {
    bool seated = false;      // true: có vé; false: đã vào hàng chờ
    Ticket ticket;            // hợp lệ khi seated == true
    int queuePosition = 0;    // vị trí trong hàng (từ 1) khi seated == false
};

struct CancelResult {
    Ticket cancelled;
    long refund = 0;
    std::vector<Ticket> granted;  // vé mới tạo cho khách trong hàng chờ nhờ ghế vừa trống
};

class BookingService {
public:
    explicit BookingService(SeatMap& seats);

    BookResult book(const std::string& name, SeatType type);
    CancelResult cancel(int ticketId);      // ném std::invalid_argument nếu không có vé hoặc đã hủy

    Ticket* findTicket(int id);             // nullptr nếu không có
    const std::vector<Ticket>& tickets() const { return tickets_; }
    std::vector<Customer> queueSnapshot() const;  // đầu hàng ở phần tử 0

    // Dùng cho D khi nạp dữ liệu từ file.
    void restore(std::vector<Ticket> tickets, std::vector<Customer> queue);

private:
    // Cấp ghế cho hàng chờ sau khi có ghế trống. Trả về các vé mới tạo.
    std::vector<Ticket> serveQueue();
    Ticket makeTicket(const std::string& name, SeatType type, Position pos);

    SeatMap& seats_;
    std::vector<Ticket> tickets_;
    Deque<Customer> waiting_;
    int nextTicketId_ = 1;
    int nextCustomerId_ = 1;
};
