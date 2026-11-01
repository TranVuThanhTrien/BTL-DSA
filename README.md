# CineQ - hệ thống bán vé xem phim (BTL DSA)

Backend C++17 (REST API) + giao diện web HTML/CSS/JavaScript thuần. Hàng chờ ghế dùng **Deque tự cài** (danh sách liên kết đôi, không dùng `std::deque`). Dữ liệu lưu CSV trong `data/`.

## Chạy
Cần CMake 3.14+ và trình biên dịch C++17.
```
cmake -S . -B build
cmake --build build
ctest --test-dir build        # 7 nhóm kiểm thử
./build/server 8080           # rồi mở http://localhost:8080
```
Chạy từ thư mục gốc dự án (server đọc `./web` và `./data`). Windows MinGW: `cmake -S . -B build -G "MinGW Makefiles"`, chạy `.\build\server.exe 8080`. Terminal tiếng Việt: `chcp 65001`.
Lần chạy đầu tự tạo dữ liệu mẫu: 6 phim, 3 phòng, các suất trong 4 ngày tới. Muốn tạo lại, tắt server và xóa mọi file trong `data/` (kể cả thư mục `data/shows`).

## Tài khoản
| Tên đăng nhập | Mật khẩu | Vai trò |
|---|---|---|
| khach | khach123 | Khách hàng |
| admin | admin123 | Quản trị / nhân viên quầy |

Khách chưa đăng nhập vẫn xem phim, lịch chiếu, sơ đồ ghế, tin tức; chỉ phải đăng nhập khi bấm giữ ghế.

## Nghiệp vụ
- Nhiều phim, nhiều phòng, nhiều suất; mỗi suất có sơ đồ ghế, giá thường/VIP riêng và một Deque hàng chờ riêng.
- Chọn nhiều ghế (tối đa 8 mỗi đơn) hoặc "Chọn ghế liền kề" (ưu tiên liền hàng, gần giữa phòng).
- Giữ ghế 10 phút khi chưa thanh toán; hết hạn tự nhả ghế và giao cho người đầu hàng chờ.
- Bắp nước, mã giảm giá (CINE10, HSSV15, VUIVE20), xác nhận độ tuổi với phim T13/T16/T18.
- Thanh toán: chuyển khoản QR, ví, thẻ (mô phỏng, không thu tiền thật); tiền mặt tại quầy dành cho admin, có tính tiền thối.
- Vé điện tử có mã QR, in được.
- Ngừng bán trước giờ chiếu 10 phút. Khách tự hủy vé đã thanh toán trước giờ chiếu ít nhất 2 tiếng, hoàn 80%; admin hủy được mọi lúc trước khi soát vé.
- Hết ghế một loại: vào danh sách chờ. VIP chen lên đầu hàng (`pushFront`), khách thường vào cuối (`pushBack`). Có ghế trống thì giao cho người đầu tiên cần đúng loại ghế (`removeFirstIf` lấy giữa hàng) và giữ 10 phút để thanh toán.
- Soát vé theo mã đơn (mở trước giờ chiếu 60 phút, mỗi vé chỉ dùng một lần).
- Admin: báo cáo doanh thu theo phim, bắp nước, mức lấp đầy từng suất; quản lý phim và suất; đăng tin; xuất đơn hàng ra CSV; đặt lại dữ liệu bán vé.
- Tên người trong hàng chờ bị che với khách khác.

## Cấu trúc
```
src/core/     Deque.h, SeatMap, BookingService (hàng chờ), Types.h
src/box/      BoxOffice: phim, suất, đơn hàng, giữ ghế, thanh toán, hủy, soát vé, thống kê
src/storage/  Storage (CSV), Ticketing
src/server/   main_server.cpp (REST API), Auth, News, Sha256
src/console/  bản dòng lệnh một suất chiếu, dữ liệu riêng, dùng khi web lỗi
tests/        Deque, SeatMap, Booking, Storage, Auth, News, Box
web/          index.html, style.css, app.js, qrcode.js (MIT, thư viện tạo QR)
third_party/  cpp-httplib, nlohmann/json
```

## API
Token đăng nhập gửi trong header `X-Token`.
GET `/api/catalog` `/api/shows?movie=` `/api/show?id=` `/api/news` `/api/me` `/api/orders` `/api/stats`
POST `/api/quote` `/api/register` `/api/login` `/api/logout` `/api/orders` `/api/orders/pay` `/api/orders/cancel` `/api/queue/join` `/api/queue/leave` `/api/checkin` `/api/movies/save|delete` `/api/showtimes/save|delete` `/api/news/create|update|delete` `/api/reset`

## Giới hạn
Thanh toán là mô phỏng. Mật khẩu băm SHA-256 + salt (đủ cho bài tập). Token phiên giữ trong bộ nhớ nên khởi động lại server phải đăng nhập lại. Mã khuyến mãi và combo nằm cố định trong code.


