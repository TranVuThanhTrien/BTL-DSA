// Sơ đồ ghế một suất chiếu. Người phụ trách: B
// Các hàng cuối là ghế VIP, các hàng đầu là ghế thường.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Types.h"

class SeatMap {
public:
    // vipRows: số hàng VIP tính từ hàng cuối. Ví dụ SeatMap(6, 8, 2).
    SeatMap(int rows, int cols, int vipRows);

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    SeatType typeAt(int r, int c) const;   // ghế VIP nếu r >= rows_ - vipRows_
    bool inRange(int r, int c) const;
    bool isFree(int r, int c) const;       // false nếu ngoài phạm vi

    bool occupy(int r, int c);             // false nếu ngoài phạm vi hoặc đã có người
    void release(int r, int c);            // trả ghế về trống

    // Tìm ghế trống đúng loại, gần tâm phòng nhất. O(rows * cols).
    std::optional<Position> findFree(SeatType type) const;
    int freeCount(SeatType type) const;

    std::string render() const;            // vẽ sơ đồ dạng chữ cho bản console
    static std::string seatName(int r, int c);  // (0,0) -> "A1"

private:
    int rows_;
    int cols_;
    int vipRows_;
    std::vector<std::vector<bool>> taken_;
};
