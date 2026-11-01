// Kiểm thử băm SHA-256, tài khoản và đăng nhập.
#include <cstdio>

#include "../src/server/Auth.h"
#include "../src/server/Sha256.h"
#include "check.h"

static void sha256_known_vectors() {
    CHECK(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");   // 2 khối
}

static void seed_users_can_login() {
    const std::string path = "test_users.csv";
    std::remove(path.c_str());
    AuthService a(path);
    auto ok = a.login("admin", "admin123");
    CHECK(ok.ok && ok.user.isAdmin());
    CHECK(a.login("KHACH", "khach123").ok);          // không phân biệt hoa thường ở tên đăng nhập
    CHECK(!a.login("admin", "sai").ok);
    CHECK(!a.login("khongco", "x").ok);
    std::remove(path.c_str());
}

static void register_validates_input() {
    const std::string path = "test_users2.csv";
    std::remove(path.c_str());
    AuthService a(path);
    CHECK(!a.registerUser("A", "an_01", "matkhau1", "matkhau1").ok);        // họ tên ngắn
    CHECK(!a.registerUser("An", "A!", "matkhau1", "matkhau1").ok);          // tên đăng nhập sai
    CHECK(!a.registerUser("An", "an_01", "123", "123").ok);                 // mật khẩu ngắn
    CHECK(!a.registerUser("An", "an_01", "matkhau1", "khac").ok);           // không khớp
    CHECK(!a.registerUser("An", "admin", "matkhau1", "matkhau1").ok);       // trùng
    auto r = a.registerUser("Nguyễn An", "an_01", "matkhau1", "matkhau1");
    CHECK(r.ok && !r.token.empty() && !r.user.isAdmin());
    std::remove(path.c_str());
}

static void sessions_and_logout() {
    const std::string path = "test_users3.csv";
    std::remove(path.c_str());
    AuthService a(path);
    auto r = a.login("khach", "khach123");
    CHECK(a.userByToken(r.token).has_value());
    CHECK(!a.userByToken("token-gia").has_value());
    a.logout(r.token);
    CHECK(!a.userByToken(r.token).has_value());
    std::remove(path.c_str());
}

static void users_persist_between_runs() {
    const std::string path = "test_users4.csv";
    std::remove(path.c_str());
    { AuthService a(path); a.registerUser("Lê Bình", "binh", "matkhau1", "matkhau1"); }
    AuthService b(path);
    CHECK(b.login("binh", "matkhau1").ok);
    std::remove(path.c_str());
}

int main() {
    RUN(sha256_known_vectors);
    RUN(seed_users_can_login);
    RUN(register_validates_input);
    RUN(sessions_and_logout);
    RUN(users_persist_between_runs);
    TEST_MAIN_END();
}


