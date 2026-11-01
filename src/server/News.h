// Tin tức và thông báo liên quan đến phim. Ai cũng đọc được, chỉ quản trị được đăng, sửa, xóa.
//
// data/news.csv : id,category,title,body,movie,pinned,ts,author   (xuống dòng trong nội dung ghi thành \n)
#pragma once
#include <string>
#include <vector>

struct NewsItem {
    int id = 0;
    std::string category;
    std::string title;
    std::string body;
    std::string movie;  // tên phim liên quan, có thể rỗng
    bool pinned = false;
    long long ts = 0;   // giây kể từ 1970
    std::string author;
};

struct NewsInput {
    std::string category, title, body, movie;
    bool pinned = false;
};

const std::vector<std::string>& newsCategories();  // Phim mới, Lịch chiếu, Khuyến mãi, Thông báo

class NewsService {
public:
    // Nạp news.csv; chưa có thì tạo vài tin mẫu.
    explicit NewsService(std::string path);

    // category rỗng hoặc "all" lấy hết. Tin ghim lên đầu, sau đó mới nhất trước.
    std::vector<NewsItem> list(const std::string& category = "") const;
    size_t count() const { return items_.size(); }

    // Các hàm ghi ném std::invalid_argument kèm thông báo tiếng Việt nếu dữ liệu sai.
    NewsItem create(const NewsInput& in, const std::string& author);
    NewsItem update(int id, const NewsInput& in);
    void remove(int id);

private:
    void save() const;
    static NewsInput validated(const NewsInput& in);

    std::string path_;
    std::vector<NewsItem> items_;
    int nextId_ = 1;
};


