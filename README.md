# Quầy vé xem phim và hàng chờ (ĐT19)

Hệ thống đặt vé xem phim bằng C++. Hàng chờ khi hết ghế dùng Deque tự cài bằng danh sách
liên kết đôi (không dùng std::deque). Giao diện web nói chuyện với server C++ qua API.

## Build và chạy
Bản console (không cần thư viện ngoài):

    g++ -std=c++17 -Isrc src/console/main_console.cpp src/core/*.cpp src/storage/*.cpp -o console
    ./console

Bản web (cần third_party/httplib.h và third_party/json.hpp):

    g++ -std=c++17 -Isrc src/server/main_server.cpp src/core/*.cpp src/storage/*.cpp -o server -pthread
    ./server          # mở http://localhost:8080

Trên Windows (MinGW) thêm -lws2_32 khi biên dịch server. Hoặc dùng CMake:

    cmake -S . -B build && cmake --build build && ctest --test-dir build

## Thành viên và module
| Người | Module |
|---|---|
| A | Deque, API server |
| B | Sơ đồ ghế, tìm chỗ trống |
| C | Điều phối vé, hủy vé, hàng chờ |
| D | Lưu file, thanh toán, xuất vé, console |
| E | Giao diện web |

<!-- TODO (D): mô tả đề tài, ảnh chụp màn hình, giải thích định dạng file dữ liệu -->
<!-- TODO (D): mục "Đóng gói và phát hành": cách tạo thư mục phát hành (server.exe + web/ + data/), nén zip;
     cách mở trên điện thoại (cùng Wi-Fi, http://<IP máy>:cổng, chọn "Thêm vào màn hình chính");
     cảnh báo Windows SmartScreen vì file chưa ký số (bấm "Thông tin thêm" rồi "Vẫn chạy") -->
