// Sơ đồ ghế của suất chiếu. Ghế VIP bắt đầu từ hàng D, các hàng còn lại là ghế thường.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Types.h"

class SeatMap {
public:
    // vipRows: số hàng VIP, tính từ hàng D trở xuống. Ví dụ SeatMap(5, 8, 1) -> VIP ở hàng D.
    SeatMap(int rows, int cols, int vipRows);

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    SeatType typeAt(int r, int c) const;  // VIP nếu 3 <= r < 3 + vipRows_ (hàng D trở xuống)
    bool inRange(int r, int c) const;
    bool isFree(int r, int c) const;      // false nếu ngoài phạm vi

    bool occupy(int r, int c);            // false nếu ngoài phạm vi hoặc đã có người
    void release(int r, int c);           // trả ghế về trống
    void clear();                         // trả mọi ghế về trống

    // Ghế trống đúng loại gần tâm phòng nhất. O(rows * cols).
    std::optional<Position> findFree(SeatType type) const;
    int freeCount(SeatType type) const;

    // Tìm count ghế trống đúng loại: ưu tiên liền kề cùng hàng gần giữa phòng,
    // không có thì lấy các ghế lẻ gần tâm nhất. Trả về rỗng nếu không đủ ghế.
    std::vector<Position> findBlock(SeatType type, int count) const;

    std::string render() const;                 // sơ đồ dạng chữ cho bản console
    static std::string seatName(int r, int c);  // (0,0) -> "A1"

private:
    int rows_;
    int cols_;
    int vipRows_;
    std::vector<std::vector<bool>> taken_;
};
