// Tài khoản, đăng nhập, phiên làm việc, phân quyền (khách / quản trị).
// Lưu ý: đây là đăng nhập đơn giản để phân quyền khi demo. Mật khẩu được băm SHA-256 kèm muối,
// nhưng dữ liệu nằm trong file CSV và chưa có HTTPS nên không dùng cho môi trường thật.
//
// data/users.csv : username,fullName,role,salt,hash
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

struct User {
    std::string username;
    std::string fullName;
    std::string role;  // "admin" hoặc "user"
    std::string salt;
    std::string hash;
    bool isAdmin() const { return role == "admin"; }
};

struct AuthResult {
    bool ok = false;
    std::string error;  // thông báo tiếng Việt cho người dùng
    std::string token;
    User user;
};

class AuthService {
public:
    // Nạp users.csv; chưa có thì tạo sẵn admin/admin123 và khach/khach123.
    explicit AuthService(std::string path);

    AuthResult registerUser(const std::string& fullName, const std::string& username,
                            const std::string& password, const std::string& confirm);
    AuthResult login(const std::string& username, const std::string& password);
    void logout(const std::string& token);
    std::optional<User> userByToken(const std::string& token) const;

private:
    void save() const;
    static std::string randomHex(int bytes);
    std::string startSession(const std::string& username);

    std::string path_;
    std::vector<User> users_;
    std::map<std::string, std::string> sessions_;  // token -> username
};
