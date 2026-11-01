#include "Ticketing.h"

#include <cstdio>
#include <sstream>

#include "../core/SeatMap.h"
#include "Storage.h"

std::string formatMoney(long amount) {
    std::string digits = std::to_string(amount < 0 ? -amount : amount), out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count && count % 3 == 0) out.insert(out.begin(), '.');
        out.insert(out.begin(), *it);
        ++count;
    }
    return (amount < 0 ? "-" : "") + out + " đ";
}

std::string ticketCode(int id) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "VE%04d", id);
    return buf;
}

PaymentResult payTicket(Ticket& ticket, long cash) {
    PaymentResult r;
    if (ticket.status == TicketStatus::Paid) { r.message = "Vé này đã được thanh toán."; return r; }
    if (ticket.status == TicketStatus::Cancelled) { r.message = "Vé này đã bị hủy, không thể thanh toán."; return r; }
    if (cash < ticket.price) {
        r.message = "Tiền khách đưa chưa đủ, còn thiếu " + formatMoney(ticket.price - cash) + ".";
        return r;
    }
    ticket.status = TicketStatus::Paid;
    r.ok = true;
    r.change = cash - ticket.price;
    return r;
}

std::string ticketText(const Ticket& t, const std::string& movie, const std::string& showTime,
                       const std::string& room) {
    std::ostringstream os;
    os << "=========== VÉ XEM PHIM ===========\n"
       << "Mã vé      : " << ticketCode(t.id) << "\n"
       << "Khách      : " << t.customerName << "\n"
       << "Phim       : " << movie << "\n"
       << "Suất chiếu : " << showTime << " · " << room << "\n"
       << "Ghế        : " << SeatMap::seatName(t.row, t.col) << (t.type == SeatType::Vip ? " (VIP)" : " (Thường)") << "\n"
       << "Giá vé     : " << formatMoney(t.price) << "\n"
       << "===================================\n";
    return os.str();
}

void writeTicketFile(const std::string& path, const Ticket& t, const std::string& movie,
                     const std::string& showTime, const std::string& room) {
    writeFileAtomic(path, ticketText(t, movie, showTime, room));
}
