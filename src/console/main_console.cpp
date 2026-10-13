// Bản demo console. Người phụ trách: D
// Chạy được độc lập với web, dùng làm phương án dự phòng khi demo.
#include <iostream>
#include <string>

#include "../core/BookingService.h"
#include "../core/SeatMap.h"
#include "../storage/Storage.h"
#include "../storage/Ticketing.h"

static const char* kTicketsFile = "data/tickets.csv";
static const char* kQueueFile = "data/queue.csv";

static void printMenu() {
    std::cout << "\n=== QUAY VE XEM PHIM ===\n"
                 "1. Xem so do ghe\n"
                 "2. Dat ve\n"
                 "3. Thanh toan\n"
                 "4. Huy ve\n"
                 "5. Xem hang cho\n"
                 "6. Luu du lieu\n"
                 "0. Thoat\n"
                 "Chon: ";
}

int main() {
    SeatMap seats(6, 8, 2);
    BookingService service(seats);

    // TODO: nạp dữ liệu cũ: service.restore(loadTickets(kTicketsFile), loadQueue(kQueueFile));
    (void)kTicketsFile;
    (void)kQueueFile;

    int choice = -1;
    while (choice != 0) {
        printMenu();
        if (!(std::cin >> choice)) break;  // hết dữ liệu nhập hoặc nhập chữ
        try {
            switch (choice) {
                case 1: std::cout << seats.render(); break;
                // TODO: case 2 .. 6, mỗi chức năng đọc dữ liệu nhập, kiểm tra đầu vào,
                //       gọi service / payTicket / saveTickets và in kết quả bằng tiếng Việt.
                case 0: break;
                default: std::cout << "Lua chon khong hop le.\n";
            }
        } catch (const std::exception& e) {
            // Mọi lỗi (hết ghế, vé không tồn tại, không ghi được file...) đều báo ở đây,
            // chương trình không được thoát đột ngột khi demo.
            std::cout << "Loi: " << e.what() << "\n";
        }
    }
    return 0;
}
