#include "BookingService.h"

#include <algorithm>
#include <ctime>
#include <stdexcept>

BookingService::BookingService(SeatMap& seats) : seats_(seats) {}

Ticket BookingService::makeTicket(const std::string& name, SeatType type, Position pos,
                                  const std::string& owner) {
    Ticket t;
    t.id = nextTicketId_++;
    t.customerName = name;
    t.type = type;
    t.row = pos.row;
    t.col = pos.col;
    t.price = priceOf(type);
    t.status = TicketStatus::Unpaid;
    t.createdAt = std::time(nullptr);
    t.owner = owner;
    return t;
}

BookResult BookingService::book(const std::string& name, SeatType type, const std::string& owner,
                                std::optional<Position> preferred) {
    BookResult result;
    std::optional<Position> pos;
    if (preferred && seats_.isFree(preferred->row, preferred->col) &&
        seats_.typeAt(preferred->row, preferred->col) == type)
        pos = preferred;
    else
        pos = seats_.findFree(type);

    if (pos) {
        seats_.occupy(pos->row, pos->col);
        result.ticket = makeTicket(name, type, *pos, owner);
        tickets_.push_back(result.ticket);
        result.seated = true;
        return result;
    }

    // Hết ghế: VIP vào đầu hàng, thường vào cuối hàng.
    Customer cu;
    cu.id = nextCustomerId_++;
    cu.name = name;
    cu.type = type;
    cu.owner = owner;
    if (type == SeatType::Vip) {
        waiting_.pushFront(cu);
        result.queuePosition = 1;
    } else {
        waiting_.pushBack(cu);
        result.queuePosition = static_cast<int>(waiting_.size());
    }
    return result;
}

CancelResult BookingService::cancel(int ticketId) {
    Ticket* t = findTicket(ticketId);
    if (!t || t->status == TicketStatus::Cancelled)
        throw std::invalid_argument("Khong tim thay ve hoac ve da bi huy");

    CancelResult result;
    result.refund = (t->status == TicketStatus::Paid) ? t->price * REFUND_PERCENT / 100 : 0;
    t->status = TicketStatus::Cancelled;
    t->refund = result.refund;
    seats_.release(t->row, t->col);
    result.cancelled = *t;
    result.granted = serveQueue();
    return result;
}

std::vector<Ticket> BookingService::serveQueue() {
    std::vector<Ticket> granted;
    Customer cu;
    // Mỗi vòng gỡ khách đầu tiên (tính từ đầu hàng) có ghế đúng loại. Lặp đến khi hết ghế phù hợp.
    while (waiting_.removeFirstIf(
        [this](const Customer& c) { return seats_.findFree(c.type).has_value(); }, &cu)) {
        const Position pos = *seats_.findFree(cu.type);
        seats_.occupy(pos.row, pos.col);
        Ticket t = makeTicket(cu.name, cu.type, pos, cu.owner);
        tickets_.push_back(t);
        granted.push_back(t);
    }
    return granted;
}

bool BookingService::leaveQueue(int customerId, Customer* removed) {
    return waiting_.removeFirstIf([customerId](const Customer& c) { return c.id == customerId; },
                                  removed);
}

Ticket* BookingService::findTicket(int id) {
    for (auto& t : tickets_)
        if (t.id == id) return &t;
    return nullptr;
}

const Ticket* BookingService::findTicket(int id) const {
    for (const auto& t : tickets_)
        if (t.id == id) return &t;
    return nullptr;
}

std::vector<Customer> BookingService::queueSnapshot() const { return waiting_.toVector(); }

void BookingService::restore(std::vector<Ticket> tickets, std::vector<Customer> queue) {
    seats_.clear();
    waiting_.clear();
    tickets_ = std::move(tickets);
    int maxTicket = 0, maxCustomer = 0;
    for (const auto& t : tickets_) {
        maxTicket = std::max(maxTicket, t.id);
        if (t.status == TicketStatus::Cancelled) continue;
        if (!seats_.occupy(t.row, t.col))
            throw std::runtime_error("Du lieu ve khong hop le: ghe " + SeatMap::seatName(t.row, t.col) +
                                     " bi trung hoac nam ngoai phong");
    }
    for (const auto& c : queue) {
        maxCustomer = std::max(maxCustomer, c.id);
        waiting_.pushBack(c);
    }
    nextTicketId_ = maxTicket + 1;
    nextCustomerId_ = maxCustomer + 1;
}


