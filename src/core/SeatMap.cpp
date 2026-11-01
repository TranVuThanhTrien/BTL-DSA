#include "SeatMap.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

SeatMap::SeatMap(int rows, int cols, int vipRows)
    : rows_(rows), cols_(cols), vipRows_(vipRows),
      taken_(rows > 0 ? rows : 0, std::vector<bool>(cols > 0 ? cols : 0, false)) {
    if (rows <= 0 || cols <= 0 || vipRows < 0 || vipRows > rows)
        throw std::invalid_argument("Kich thuoc phong khong hop le");
}

bool SeatMap::inRange(int r, int c) const {
    return r >= 0 && r < rows_ && c >= 0 && c < cols_;
}

SeatType SeatMap::typeAt(int r, int) const {
    const int vipStart = 3;  // hàng D (A=0, B=1, C=2, D=3)
    return (r >= vipStart && r < vipStart + vipRows_) ? SeatType::Vip : SeatType::Normal;
}

bool SeatMap::isFree(int r, int c) const { return inRange(r, c) && !taken_[r][c]; }

bool SeatMap::occupy(int r, int c) {
    if (!isFree(r, c)) return false;
    taken_[r][c] = true;
    return true;
}

void SeatMap::release(int r, int c) {
    if (inRange(r, c)) taken_[r][c] = false;
}

void SeatMap::clear() {
    for (auto& row : taken_) std::fill(row.begin(), row.end(), false);
}

std::optional<Position> SeatMap::findFree(SeatType type) const {
    int minR = rows_, maxR = -1;
    for (int r = 0; r < rows_; ++r) {
        if (typeAt(r, 0) != type) continue;
        minR = std::min(minR, r);
        maxR = std::max(maxR, r);
    }
    if (maxR < 0) return std::nullopt;  // phòng không có loại ghế này
    const double centerCol = (cols_ - 1) / 2.0, centerRow = (minR + maxR) / 2.0;
    std::optional<Position> best;
    double bestScore = 1e18;
    for (int r = minR; r <= maxR; ++r) {
        if (typeAt(r, 0) != type) continue;  // bỏ qua hàng khác loại nằm xen giữa
        for (int c = 0; c < cols_; ++c) {
            if (taken_[r][c]) continue;
            // ưu tiên gần giữa dãy; hàng gần tâm khu ghế được cộng điểm nhẹ hơn
            const double score = std::abs(c - centerCol) + 0.6 * std::abs(r - centerRow) + c * 0.001;
            if (score < bestScore) { bestScore = score; best = Position{r, c}; }
        }
    }
    return best;
}

int SeatMap::freeCount(SeatType type) const {
    int n = 0;
    for (int r = 0; r < rows_; ++r)
        for (int c = 0; c < cols_; ++c)
            if (typeAt(r, c) == type && !taken_[r][c]) ++n;
    return n;
}

std::vector<Position> SeatMap::findBlock(SeatType type, int count) const {
    if (count <= 0 || freeCount(type) < count) return {};
    const double centerCol = (cols_ - 1) / 2.0;
    double bestScore = 1e18;
    std::vector<Position> best;
    for (int r = 0; r < rows_; ++r) {
        if (typeAt(r, 0) != type) continue;
        for (int c = 0; c + count <= cols_; ++c) {
            bool ok = true;
            for (int k = 0; k < count && ok; ++k) ok = !taken_[r][c + k];
            if (!ok) continue;
            // trung tâm khối càng gần giữa dãy càng tốt; hàng xa màn hình bị trừ nhẹ
            const double score = std::abs(c + (count - 1) / 2.0 - centerCol) + r * 0.01;
            if (score < bestScore) {
                bestScore = score;
                best.clear();
                for (int k = 0; k < count; ++k) best.push_back({r, c + k});
            }
        }
    }
    if (!best.empty()) return best;
    // Không có khối liền kề: lấy lần lượt ghế lẻ gần tâm nhất.
    std::vector<std::pair<double, Position>> all;
    int minR = rows_, maxR = -1;
    for (int r = 0; r < rows_; ++r)
        if (typeAt(r, 0) == type) { minR = std::min(minR, r); maxR = std::max(maxR, r); }
    const double centerRow = (minR + maxR) / 2.0;
    for (int r = minR; r <= maxR; ++r) {
        if (typeAt(r, 0) != type) continue;  // bỏ qua hàng khác loại nằm xen giữa
        for (int c = 0; c < cols_; ++c)
            if (!taken_[r][c])
                all.push_back({std::abs(c - centerCol) + 0.6 * std::abs(r - centerRow) + c * 0.001, {r, c}});
    }
    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (int i = 0; i < count; ++i) best.push_back(all[i].second);
    return best;
}

std::string SeatMap::render() const {
    std::ostringstream os;
    os << "    ";
    for (int c = 0; c < cols_; ++c) os << (c + 1) << (c + 1 < 10 ? "  " : " ");
    os << "\n";
    for (int r = 0; r < rows_; ++r) {
        os << " " << static_cast<char>('A' + r) << "  ";
        for (int c = 0; c < cols_; ++c) {
            if (taken_[r][c]) os << "X  ";
            else os << (typeAt(r, c) == SeatType::Vip ? "V  " : ".  ");
        }
        os << "\n";
    }
    os << "    (.: ghe thuong trong   V: ghe VIP trong   X: da dat)\n";
    return os.str();
}

std::string SeatMap::seatName(int r, int c) {
    return std::string(1, static_cast<char>('A' + r)) + std::to_string(c + 1);
}
