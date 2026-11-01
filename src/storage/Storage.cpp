#include "Storage.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::string toString(SeatType t) { return t == SeatType::Vip ? "VIP" : "NORMAL"; }

std::string toString(TicketStatus s) {
    switch (s) {
        case TicketStatus::Paid: return "PAID";
        case TicketStatus::Cancelled: return "CANCELLED";
        default: return "UNPAID";
    }
}

SeatType seatTypeFromString(const std::string& s) {
    if (s == "VIP") return SeatType::Vip;
    if (s == "NORMAL") return SeatType::Normal;
    throw StorageError("Loai ghe khong hop le: " + s);
}

TicketStatus statusFromString(const std::string& s) {
    if (s == "UNPAID") return TicketStatus::Unpaid;
    if (s == "PAID") return TicketStatus::Paid;
    if (s == "CANCELLED") return TicketStatus::Cancelled;
    throw StorageError("Trang thai ve khong hop le: " + s);
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

std::string csvQuote(const std::string& f) {
    if (f.find_first_of(",\"\n\r") == std::string::npos) return f;
    std::string out = "\"";
    for (char ch : f) {
        if (ch == '"') out += "\"\"";
        else out += ch;
    }
    return out + "\"";
}

std::vector<std::string> csvSplit(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (inQuotes) {
            if (ch == '"' && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
            else if (ch == '"') inQuotes = false;
            else cur += ch;
        } else if (ch == '"') {
            inQuotes = true;
        } else if (ch == ',') {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    fields.push_back(cur);
    return fields;
}

void writeFileAtomic(const std::string& path, const std::string& content) {
    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw StorageError("Khong mo duoc file de ghi: " + tmp);
        out << content;
        out.flush();
        if (!out) throw StorageError("Ghi file that bai (day o dia?): " + tmp);
    }
    fs::rename(tmp, target, ec);
    if (ec) {                       // Windows không cho đổi tên đè lên file có sẵn
        fs::remove(target, ec);
        ec.clear();
        fs::rename(tmp, target, ec);
        if (ec) throw StorageError("Khong doi ten duoc file tam: " + ec.message());
    }
}

std::vector<std::string> readLines(const std::string& path, bool* existed) {
    std::ifstream in(path, std::ios::binary);
    if (existed) *existed = static_cast<bool>(in);
    std::vector<std::string> lines;
    if (!in) return lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static long parseLong(const std::string& s, const char* what, std::size_t lineNo) {
    try {
        std::size_t used = 0;
        const long v = std::stol(s, &used);
        if (used != s.size()) throw std::invalid_argument("x");
        return v;
    } catch (const std::exception&) {
        throw StorageError("Dong " + std::to_string(lineNo) + ": " + what + " khong phai so hop le: '" + s + "'");
    }
}

void saveTickets(const std::string& path, const std::vector<Ticket>& tickets) {
    std::ostringstream os;
    os << "id,name,type,row,col,price,status,createdAt,owner,refund,orderId\n";
    for (const auto& t : tickets)
        os << t.id << ',' << csvQuote(t.customerName) << ',' << toString(t.type) << ',' << t.row << ','
           << t.col << ',' << t.price << ',' << toString(t.status) << ',' << static_cast<long long>(t.createdAt)
           << ',' << csvQuote(t.owner) << ',' << t.refund << ',' << t.orderId << '\n';
    writeFileAtomic(path, os.str());
}

std::vector<Ticket> loadTickets(const std::string& path) {
    bool existed = false;
    const auto lines = readLines(path, &existed);
    std::vector<Ticket> out;
    for (std::size_t i = 1; i < lines.size(); ++i) {  // dòng 0 là tiêu đề
        if (lines[i].empty()) continue;
        const std::size_t lineNo = i + 1;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 8)
            throw StorageError("Dong " + std::to_string(lineNo) + ": thieu cot (can it nhat 8, co " +
                               std::to_string(f.size()) + ")");
        Ticket t;
        t.id = static_cast<int>(parseLong(f[0], "id", lineNo));
        t.customerName = f[1];
        t.type = seatTypeFromString(f[2]);
        t.row = static_cast<int>(parseLong(f[3], "row", lineNo));
        t.col = static_cast<int>(parseLong(f[4], "col", lineNo));
        t.price = parseLong(f[5], "price", lineNo);
        t.status = statusFromString(f[6]);
        t.createdAt = static_cast<std::time_t>(parseLong(f[7], "createdAt", lineNo));
        if (f.size() > 8) t.owner = f[8];
        if (f.size() > 9 && !f[9].empty()) t.refund = parseLong(f[9], "refund", lineNo);
        if (f.size() > 10 && !f[10].empty()) t.orderId = static_cast<int>(parseLong(f[10], "orderId", lineNo));
        if (t.row < 0 || t.col < 0 || t.id <= 0)
            throw StorageError("Dong " + std::to_string(lineNo) + ": gia tri am hoac id khong hop le");
        out.push_back(t);
    }
    return out;
}

void saveQueue(const std::string& path, const std::vector<Customer>& queue) {
    std::ostringstream os;
    os << "id,name,type,owner\n";
    for (const auto& c : queue)
        os << c.id << ',' << csvQuote(c.name) << ',' << toString(c.type) << ',' << csvQuote(c.owner) << '\n';
    writeFileAtomic(path, os.str());
}

std::vector<Customer> loadQueue(const std::string& path) {
    const auto lines = readLines(path);
    std::vector<Customer> out;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const std::size_t lineNo = i + 1;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 3)
            throw StorageError("Dong " + std::to_string(lineNo) + ": thieu cot (can it nhat 3)");
        Customer c;
        c.id = static_cast<int>(parseLong(f[0], "id", lineNo));
        c.name = f[1];
        c.type = seatTypeFromString(f[2]);
        if (f.size() > 3) c.owner = f[3];
        out.push_back(c);
    }
    return out;
}


