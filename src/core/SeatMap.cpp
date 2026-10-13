#include "SeatMap.h"

#include <stdexcept>

SeatMap::SeatMap(int rows, int cols, int vipRows)
    : rows_(rows), cols_(cols), vipRows_(vipRows),
      taken_(rows, std::vector<bool>(cols, false)) {
    if (rows <= 0 || cols <= 0 || vipRows < 0 || vipRows > rows)
        throw std::invalid_argument("Kich thuoc phong khong hop le");
}

bool SeatMap::inRange(int r, int c) const {
    return r >= 0 && r < rows_ && c >= 0 && c < cols_;
}

SeatType SeatMap::typeAt(int r, int c) const {
    (void)c;
    return r >= rows_ - vipRows_ ? SeatType::Vip : SeatType::Normal;
}

bool SeatMap::isFree(int r, int c) const {
    // TODO: false nếu ngoài phạm vi, ngược lại trả !taken_[r][c]
    (void)r; (void)c;
    return false;
}

bool SeatMap::occupy(int r, int c) {
    // TODO: chỉ đặt được khi isFree(r, c)
    (void)r; (void)c;
    return false;
}

void SeatMap::release(int r, int c) {
    // TODO: bỏ qua nếu ngoài phạm vi
    (void)r; (void)c;
}

std::optional<Position> SeatMap::findFree(SeatType type) const {
    // TODO: duyệt mọi ô đúng loại còn trống, chấm điểm theo khoảng cách tới giữa hàng
    //       và giữa dãy (ghế đẹp là ghế gần tâm), chọn ô điểm thấp nhất.
    //       Trả về std::nullopt nếu hết ghế loại đó.
    (void)type;
    return std::nullopt;
}

int SeatMap::freeCount(SeatType type) const {
    // TODO
    (void)type;
    return 0;
}

std::string SeatMap::render() const {
    // TODO: mỗi hàng một dòng, ví dụ "A [ ][ ][X][ ]" ; X = đã đặt, V = VIP trống
    return "";
}

std::string SeatMap::seatName(int r, int c) {
    return std::string(1, static_cast<char>('A' + r)) + std::to_string(c + 1);
}
