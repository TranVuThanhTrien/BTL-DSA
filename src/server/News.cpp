#include "News.h"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <stdexcept>

#include "../storage/Storage.h"

namespace {
std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}
std::size_t utf8Length(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}
std::string encodeNl(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else if (c != '\r') out += c;
    }
    return out;
}
std::string decodeNl(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            out += s[i + 1] == 'n' ? '\n' : s[i + 1];
            ++i;
        } else out += s[i];
    }
    return out;
}
}  // namespace

const std::vector<std::string>& newsCategories() {
    static const std::vector<std::string> c{"Phim mới", "Lịch chiếu", "Khuyến mãi", "Thông báo"};
    return c;
}

NewsInput NewsService::validated(const NewsInput& in) {
    NewsInput out = in;
    out.title = trim(in.title);
    out.body = trim(in.body);
    out.movie = trim(in.movie);
    if (utf8Length(out.title) < 3) throw std::invalid_argument("Nhập tiêu đề (ít nhất 3 ký tự).");
    if (utf8Length(out.title) > 100) throw std::invalid_argument("Tiêu đề tối đa 100 ký tự.");
    if (utf8Length(out.body) < 5) throw std::invalid_argument("Nhập nội dung thông báo.");
    if (utf8Length(out.body) > 1500) throw std::invalid_argument("Nội dung tối đa 1500 ký tự.");
    if (utf8Length(out.movie) > 60) throw std::invalid_argument("Tên phim tối đa 60 ký tự.");
    const auto& cats = newsCategories();
    if (std::find(cats.begin(), cats.end(), out.category) == cats.end()) out.category = "Thông báo";
    return out;
}

NewsService::NewsService(std::string path) : path_(std::move(path)) {
    bool existed = false;
    const auto lines = readLines(path_, &existed);
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 8) throw StorageError("news.csv dong " + std::to_string(i + 1) + ": thieu cot");
        NewsItem n;
        try {
            n.id = std::stoi(f[0]);
            n.ts = std::stoll(f[6]);
        } catch (const std::exception&) {
            throw StorageError("news.csv dong " + std::to_string(i + 1) + ": id hoac thoi gian khong phai so");
        }
        n.category = f[1];
        n.title = f[2];
        n.body = decodeNl(f[3]);
        n.movie = f[4];
        n.pinned = f[5] == "1";
        n.author = f[7];
        items_.push_back(n);
        nextId_ = std::max(nextId_, n.id + 1);
    }
    if (!existed) {
        const long long now = static_cast<long long>(std::time(nullptr));
        items_ = {
            {1, "Thông báo", "Chính sách hủy vé và hoàn tiền",
             "Vé đã thanh toán có thể hủy bất cứ lúc nào và được hoàn 80% giá vé. Vé chưa thanh toán hủy miễn phí. "
             "Khi bạn hủy, ghế được chuyển ngay cho khách đầu hàng chờ.", "", true, now - 2 * 3600, "Quản trị viên"},
            {2, "Phim mới", "Đêm Sài Gòn Mưa khởi chiếu",
             "Phim tình cảm dài 112 phút đã lên lịch tại Phòng 1. Xem sơ đồ ghế và đặt chỗ ngay để chọn được vị trí đẹp.",
             "Đêm Sài Gòn Mưa", false, now - 5 * 3600, "Quản trị viên"},
            {3, "Lịch chiếu", "Hết ghế? Hãy vào hàng chờ",
             "Khi suất chiếu hết ghế, bạn vẫn có thể đăng ký vào hàng chờ. Khách VIP được xếp ở đầu hàng, khách thường "
             "xếp ở cuối hàng. Có người hủy vé thì ghế được cấp theo thứ tự hàng chờ.", "", false, now - 26 * 3600, "Quản trị viên"}};
        nextId_ = 4;
        save();
    }
}

void NewsService::save() const {
    std::ostringstream os;
    os << "id,category,title,body,movie,pinned,ts,author\n";
    for (const auto& n : items_)
        os << n.id << ',' << csvQuote(n.category) << ',' << csvQuote(n.title) << ',' << csvQuote(encodeNl(n.body))
           << ',' << csvQuote(n.movie) << ',' << (n.pinned ? 1 : 0) << ',' << n.ts << ',' << csvQuote(n.author) << '\n';
    writeFileAtomic(path_, os.str());
}

std::vector<NewsItem> NewsService::list(const std::string& category) const {
    std::vector<NewsItem> out;
    for (const auto& n : items_)
        if (category.empty() || category == "all" || n.category == category) out.push_back(n);
    std::stable_sort(out.begin(), out.end(), [](const NewsItem& a, const NewsItem& b) {
        if (a.pinned != b.pinned) return a.pinned;
        return a.ts > b.ts;
    });
    return out;
}

NewsItem NewsService::create(const NewsInput& raw, const std::string& author) {
    const NewsInput in = validated(raw);
    NewsItem n{nextId_++, in.category, in.title, in.body, in.movie, in.pinned,
               static_cast<long long>(std::time(nullptr)), author};
    items_.push_back(n);
    save();
    return n;
}

NewsItem NewsService::update(int id, const NewsInput& raw) {
    const NewsInput in = validated(raw);
    for (auto& n : items_) {
        if (n.id != id) continue;
        n.category = in.category; n.title = in.title; n.body = in.body; n.movie = in.movie; n.pinned = in.pinned;
        save();
        return n;
    }
    throw std::invalid_argument("Tin này không còn tồn tại.");
}

void NewsService::remove(int id) {
    const auto it = std::remove_if(items_.begin(), items_.end(), [id](const NewsItem& n) { return n.id == id; });
    if (it == items_.end()) throw std::invalid_argument("Tin này không còn tồn tại.");
    items_.erase(it, items_.end());
    save();
}


