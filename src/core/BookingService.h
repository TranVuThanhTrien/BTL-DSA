// Điều phối vé ưu tiên/thường, hủy vé hoàn vào đầu hàng, hàng chờ.
//
// Quy tắc nghiệp vụ:
//   - Còn ghế đúng loại: tạo vé trạng thái Unpaid, giữ ghế.
//   - Hết ghế: khách VIP vào ĐẦU hàng chờ (pushFront), khách thường vào CUỐI (pushBack).
//   - Hủy vé: giải phóng ghế, rồi cấp ghế đó cho khách ĐẦU hàng phù hợp
//     (popFront; nếu người đầu hàng cần loại ghế khác thì quét tiếp và gỡ giữa chuỗi bằng removeFirstIf).
//     Khách được cấp ghế nhận vé Unpaid, giữ nguyên người đặt.
//   - Hủy vé đã thanh toán hoàn REFUND_PERCENT, vé chưa thanh toán không hoàn.
#pragma once
#include <optional>
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

    // Giá vé của suất chiếu này (mặc định PRICE_NORMAL / PRICE_VIP).
    void setPrices(long normal, long vip) { priceNormal_ = normal; priceVip_ = vip; }
    long priceOf(SeatType t) const { return t == SeatType::Vip ? priceVip_ : priceNormal_; }

    // preferred: ghế khách chọn tay. Hợp lệ (còn trống, đúng loại) thì dùng, không thì tự tìm ghế.
    BookResult book(const std::string& name, SeatType type, const std::string& owner = "",
                    std::optional<Position> preferred = std::nullopt);

    // Ném std::invalid_argument nếu không có vé hoặc vé đã hủy.
    CancelResult cancel(int ticketId);

    // Khách rời hàng chờ. Trả về true nếu gỡ được; thông tin khách ghi vào *removed.
    bool leaveQueue(int customerId, Customer* removed = nullptr);

    Ticket* findTicket(int id);
    const Ticket* findTicket(int id) const;
    const std::vector<Ticket>& tickets() const { return tickets_; }
    std::vector<Customer> queueSnapshot() const;  // đầu hàng ở phần tử 0
    SeatMap& seatMap() { return seats_; }
    const SeatMap& seatMap() const { return seats_; }

    // Nạp lại dữ liệu từ file. Ném std::runtime_error nếu hai vé cùng giữ một ghế.
    void restore(std::vector<Ticket> tickets, std::vector<Customer> queue);

private:
    std::vector<Ticket> serveQueue();
    Ticket makeTicket(const std::string& name, SeatType type, Position pos, const std::string& owner);

    SeatMap& seats_;
    std::vector<Ticket> tickets_;
    Deque<Customer> waiting_;
    long priceNormal_ = PRICE_NORMAL;
    long priceVip_ = PRICE_VIP;
    int nextTicketId_ = 1;
    int nextCustomerId_ = 1;
};
