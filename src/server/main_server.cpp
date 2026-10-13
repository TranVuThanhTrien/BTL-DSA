// API server. Người phụ trách: A
// Dùng cpp-httplib (third_party/httplib.h) và nlohmann/json (third_party/json.hpp).
// Phần lõi (src/core) không biết gì về web, để main_console vẫn chạy khi web lỗi.
//
// Hợp đồng API (E dựa vào đây để viết app.js). Mọi phản hồi là JSON.
//   GET  /api/seats   -> { rows, cols, free: {normal, vip},
//                          seats: [[ {type:"NORMAL"|"VIP", taken:bool} ...cột ]...hàng] }
//   POST /api/book    {name, type:"NORMAL"|"VIP"}
//                     -> { seated:true,  ticket:{...} }
//                      | { seated:false, queuePosition:n }
//   POST /api/pay     {ticketId, cash} -> { ok, change, message }
//   POST /api/cancel  {ticketId}       -> { refund, granted:[ticket...] }
//   GET  /api/queue   -> { size, items:[{id,name,type}...] }   (đầu hàng ở phần tử 0)
// Ticket: { id, name, type, seat:"A1", row, col, price, status:"UNPAID"|"PAID"|"CANCELLED" }
// Lỗi: HTTP 400 hoặc 404 kèm { error: "thông báo tiếng Việt" }.
#include <iostream>
#include <mutex>

#include "../../third_party/httplib.h"
#include "../../third_party/json.hpp"
#include "../core/BookingService.h"
#include "../core/SeatMap.h"
#include "../storage/Storage.h"
#include "../storage/Ticketing.h"

using json = nlohmann::json;

static SeatMap g_seats(6, 8, 2);
static BookingService g_service(g_seats);
static std::mutex g_mutex;  // httplib xử lý nhiều yêu cầu cùng lúc, mọi truy cập dữ liệu phải khóa

// TODO (A, đóng gói): đường dẫn gốc. Lấy thư mục chứa file exe làm gốc cho "web/" và "data/"
//       (không dùng thư mục đang chạy) để bấm đúp hay mở bằng shortcut vẫn thấy giao diện và dữ liệu.
//       Windows: GetModuleFileNameA; Linux: std::filesystem::canonical("/proc/self/exe").

static json ticketToJson(const Ticket& t) {
    return json{{"id", t.id},
                {"name", t.customerName},
                {"type", toString(t.type)},
                {"seat", SeatMap::seatName(t.row, t.col)},
                {"row", t.row},
                {"col", t.col},
                {"price", t.price},
                {"status", toString(t.status)}};
}

static void sendJson(httplib::Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

static void sendError(httplib::Response& res, int status, const std::string& msg) {
    sendJson(res, json{{"error", msg}}, status);
}

int main() {
    httplib::Server svr;

    // TODO (A, đóng gói): tạo thư mục data/ nếu chưa có, rồi nạp dữ liệu cũ khi khởi động (giống main_console):
    //       g_service.restore(loadTickets(...), loadQueue(...));

    svr.Get("/api/seats", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        // TODO: dựng JSON theo hợp đồng ở đầu tệp, dùng g_seats.typeAt / isFree / freeCount
        sendError(res, 501, "Chua cai dat /api/seats");
    });

    svr.Post("/api/book", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        try {
            json in = json::parse(req.body);
            // TODO: lấy name, type; kiểm tra tên không rỗng; gọi g_service.book(...);
            //       trả JSON theo hợp đồng. Nhớ lưu file sau mỗi thay đổi (D cung cấp saveTickets).
            (void)in;
            sendError(res, 501, "Chua cai dat /api/book");
        } catch (const json::exception&) {
            sendError(res, 400, "Du lieu gui len khong phai JSON hop le");
        } catch (const std::exception& e) {
            sendError(res, 400, e.what());
        }
    });

    svr.Post("/api/pay", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        (void)req;
        // TODO: findTicket -> payTicket (của D) -> trả { ok, change, message }
        sendError(res, 501, "Chua cai dat /api/pay");
    });

    svr.Post("/api/cancel", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        (void)req;
        // TODO: g_service.cancel(ticketId) -> { refund, granted }
        sendError(res, 501, "Chua cai dat /api/cancel");
    });

    svr.Get("/api/queue", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_mutex);
        // TODO: g_service.queueSnapshot() -> { size, items }
        sendError(res, 501, "Chua cai dat /api/queue");
    });

    // TODO (A, đóng gói): lưu dữ liệu sau mỗi lần đặt, thanh toán, hủy (trong lúc đang khóa g_mutex):
    //       saveTickets(...); saveQueue(...); để tắt app đột ngột cũng không mất vé.
    // TODO (A, đóng gói): đổi "./web" thành <thư mục exe>/web.
    svr.set_mount_point("/", "./web");  // phục vụ giao diện của E

    // TODO (A, đóng gói): đọc cổng từ biến môi trường PORT (mặc định 8080). Nếu cổng bận thì thử 8081..8090
    //       rồi mới báo lỗi tiếng Việt; in và mở đúng cổng thực tế đã dùng.
    // TODO (A, đóng gói): tự mở trình duyệt khi server sẵn sàng (gọi sau bind_to_port, listen ở luồng riêng):
    //       Windows: system("start http://localhost:8080"); macOS: "open"; Linux: "xdg-open".

    std::cout << "Mo trinh duyet tai http://localhost:8080\n";
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Khong mo duoc cong 8080 (co the dang bi chuong trinh khac dung)\n";
        return 1;
    }
    return 0;
}
