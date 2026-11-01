#include "Auth.h"

#include <algorithm>
#include <random>
#include <sstream>

#include "../storage/Storage.h"
#include "Sha256.h"

namespace {
std::string normalizeSpaces(const std::string& s) {
    std::istringstream in(s);
    std::string word, out;
    while (in >> word) out += (out.empty() ? "" : " ") + word;
    return out;
}
bool validUsername(const std::string& u) {
    if (u.size() < 3 || u.size() > 20) return false;
    return std::all_of(u.begin(), u.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}
}  // namespace

std::string AuthService::randomHex(int bytes) {
    static const char* hex = "0123456789abcdef";
    std::random_device rd;
    std::string out;
    for (int i = 0; i < bytes; ++i) {
        const unsigned v = rd() & 0xff;
        out += hex[v >> 4];
        out += hex[v & 15];
    }
    return out;
}

AuthService::AuthService(std::string path) : path_(std::move(path)) {
    const auto lines = readLines(path_);
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const auto f = csvSplit(lines[i]);
        if (f.size() < 5) throw StorageError("users.csv dong " + std::to_string(i + 1) + ": thieu cot");
        users_.push_back(User{f[0], f[1], f[2], f[3], f[4]});
    }
    bool changed = false;
    const struct { const char *u, *n, *r, *p; } seeds[] = {{"admin", "Quản trị viên", "admin", "admin123"},
                                                          {"khach", "Khách mẫu", "user", "khach123"}};
    for (const auto& s : seeds) {
        if (std::any_of(users_.begin(), users_.end(), [&](const User& u) { return u.username == s.u; })) continue;
        const std::string salt = randomHex(8);
        users_.push_back(User{s.u, s.n, s.r, salt, sha256Hex(salt + ":" + s.p)});
        changed = true;
    }
    if (changed) save();
}

void AuthService::save() const {
    std::ostringstream os;
    os << "username,fullName,role,salt,hash\n";
    for (const auto& u : users_)
        os << csvQuote(u.username) << ',' << csvQuote(u.fullName) << ',' << u.role << ',' << u.salt << ',' << u.hash << '\n';
    writeFileAtomic(path_, os.str());
}

std::string AuthService::startSession(const std::string& username) {
    const std::string token = randomHex(16);
    sessions_[token] = username;
    return token;
}

AuthResult AuthService::registerUser(const std::string& fullNameIn, const std::string& usernameIn,
                                     const std::string& password, const std::string& confirm) {
    AuthResult r;
    const std::string fullName = normalizeSpaces(fullNameIn), username = lower(usernameIn);
    // Đếm ký tự UTF-8 (không đếm byte tiếp nối) để tên có dấu không bị tính dài gấp đôi.
    std::size_t chars = 0;
    for (unsigned char c : fullName) if ((c & 0xC0) != 0x80) ++chars;
    if (chars < 2 || chars > 40) r.error = "Nhập họ tên (từ 2 đến 40 ký tự).";
    else if (!validUsername(username)) r.error = "Tên đăng nhập gồm 3-20 ký tự: chữ thường, số hoặc dấu gạch dưới.";
    else if (password.size() < 6) r.error = "Mật khẩu cần ít nhất 6 ký tự.";
    else if (password != confirm) r.error = "Hai mật khẩu chưa khớp.";
    else if (std::any_of(users_.begin(), users_.end(), [&](const User& u) { return u.username == username; }))
        r.error = "Tên đăng nhập đã được dùng.";
    if (!r.error.empty()) return r;

    User u{username, fullName, "user", randomHex(8), ""};
    u.hash = sha256Hex(u.salt + ":" + password);
    users_.push_back(u);
    save();
    r.ok = true;
    r.user = u;
    r.token = startSession(username);
    return r;
}

AuthResult AuthService::login(const std::string& usernameIn, const std::string& password) {
    AuthResult r;
    const std::string username = lower(usernameIn);
    auto it = std::find_if(users_.begin(), users_.end(), [&](const User& u) { return u.username == username; });
    if (it == users_.end() || sha256Hex(it->salt + ":" + password) != it->hash) {
        r.error = "Sai tên đăng nhập hoặc mật khẩu.";
        return r;
    }
    r.ok = true;
    r.user = *it;
    r.token = startSession(username);
    return r;
}

void AuthService::logout(const std::string& token) { sessions_.erase(token); }

std::optional<User> AuthService::userByToken(const std::string& token) const {
    auto s = sessions_.find(token);
    if (s == sessions_.end()) return std::nullopt;
    for (const auto& u : users_)
        if (u.username == s->second) return u;
    return std::nullopt;
}
