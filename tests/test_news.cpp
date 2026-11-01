// Kiểm thử tin tức.
#include <cstdio>

#include "../src/server/News.h"
#include "check.h"

static NewsInput input(const std::string& title, const std::string& body, bool pinned = false) {
    NewsInput n;
    n.category = "Phim mới"; n.title = title; n.body = body; n.pinned = pinned;
    return n;
}

static void seeds_when_file_missing() {
    const std::string path = "test_news1.csv";
    std::remove(path.c_str());
    NewsService s(path);
    CHECK(s.count() >= 3);
    std::remove(path.c_str());
}

static void create_validates() {
    const std::string path = "test_news2.csv";
    std::remove(path.c_str());
    NewsService s(path);
    bool a = false, b = false;
    try { s.create(input("ab", "noi dung hop le"), "Admin"); } catch (const std::invalid_argument&) { a = true; }
    try { s.create(input("Tiêu đề tốt", "x"), "Admin"); } catch (const std::invalid_argument&) { b = true; }
    CHECK(a && b);
    auto n = s.create(input("Tiêu đề tốt", "Nội dung đủ dài"), "Admin");
    CHECK(n.id > 0 && n.author == "Admin");
    std::remove(path.c_str());
}

static void pinned_first_then_newest() {
    const std::string path = "test_news3.csv";
    std::remove(path.c_str());
    NewsService s(path);
    auto pinned = s.create(input("Tin ghim", "Nội dung ghim", true), "A");
    auto all = s.list();
    CHECK(!all.empty() && all.front().id == pinned.id);
    for (std::size_t i = 2; i < all.size(); ++i)
        if (!all[i].pinned && !all[i - 1].pinned) CHECK(all[i - 1].ts >= all[i].ts);
    std::remove(path.c_str());
}

static void filter_update_remove() {
    const std::string path = "test_news4.csv";
    std::remove(path.c_str());
    NewsService s(path);
    auto n = s.create(input("Tin phim", "Nội dung phim mới"), "A");
    NewsInput e = input("Tin đã sửa", "Nội dung đã sửa xong");
    e.category = "Khuyến mãi";
    s.update(n.id, e);
    bool found = false;
    for (auto& x : s.list("Khuyến mãi")) if (x.id == n.id && x.title == "Tin đã sửa") found = true;
    CHECK(found);
    s.remove(n.id);
    bool still = false;
    for (auto& x : s.list()) if (x.id == n.id) still = true;
    CHECK(!still);
    bool threw = false;
    try { s.remove(n.id); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    std::remove(path.c_str());
}

static void multiline_body_survives_reload() {
    const std::string path = "test_news5.csv";
    std::remove(path.c_str());
    int id;
    {
        NewsService s(path);
        id = s.create(input("Nhiều dòng", "Dòng một, có dấu phẩy\nDòng hai \"trích dẫn\"\\ gạch"), "A").id;
    }
    NewsService again(path);
    std::string body;
    for (auto& x : again.list()) if (x.id == id) body = x.body;
    CHECK(body == "Dòng một, có dấu phẩy\nDòng hai \"trích dẫn\"\\ gạch");
    std::remove(path.c_str());
}

int main() {
    RUN(seeds_when_file_missing);
    RUN(create_validates);
    RUN(pinned_first_then_newest);
    RUN(filter_update_remove);
    RUN(multiline_body_survives_reload);
    TEST_MAIN_END();
}
