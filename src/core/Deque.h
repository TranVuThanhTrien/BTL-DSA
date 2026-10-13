// Deque (hàng đợi hai đầu) tự cài bằng danh sách liên kết đôi. KHÔNG dùng std::deque.
// Người phụ trách: A
//
// Bất biến cần giữ:
//   - rỗng:  head_ == nullptr, tail_ == nullptr, size_ == 0
//   - head_->prev == nullptr và tail_->next == nullptr
//   - với mọi nút n: n->next->prev == n và n->prev->next == n
//
// Độ phức tạp (ghi vào Chương 4 của báo cáo):
//   pushFront, pushBack, popFront, popBack, peek, size : O(1)
//   removeFirstIf, toVector, clear                     : O(n)
#pragma once
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <vector>

template <typename T>
class Deque {
    struct Node {
        T data;
        Node* prev;
        Node* next;
        explicit Node(const T& v) : data(v), prev(nullptr), next(nullptr) {}
    };

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;

    [[noreturn]] static void todo(const char* fn) {
        throw std::logic_error(std::string("TODO: ") + fn);
    }

public:
    Deque() = default;
    ~Deque() { clear(); }
    Deque(const Deque&) = delete;             // tránh hai Deque cùng giữ một chuỗi nút
    Deque& operator=(const Deque&) = delete;

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // Thêm vào đầu. O(1).
    void pushFront(const T& v) {
        (void)v;
        // TODO: tạo Node; nếu rỗng thì head_ = tail_ = nút mới;
        //       ngược lại nối nút mới vào trước head_ rồi dời head_. size_++.
        todo("pushFront");
    }

    // Thêm vào cuối. O(1).
    void pushBack(const T& v) {
        (void)v;
        // TODO: đối xứng với pushFront.
        todo("pushBack");
    }

    // Lấy ra và trả về phần tử đầu. Ném std::underflow_error nếu rỗng. O(1).
    T popFront() {
        // TODO: lưu giá trị, dời head_; nếu head_ thành nullptr thì tail_ = nullptr;
        //       delete nút cũ; size_--.
        todo("popFront");
    }

    // Lấy ra và trả về phần tử cuối. Ném std::underflow_error nếu rỗng. O(1).
    T popBack() {
        // TODO: đối xứng với popFront.
        todo("popBack");
    }

    // Xem phần tử đầu / cuối mà không gỡ. Ném std::underflow_error nếu rỗng.
    T& peekFront() { todo("peekFront"); }
    T& peekBack() { todo("peekBack"); }

    // Gỡ phần tử đầu tiên (tính từ head_) thỏa điều kiện, nối lại prev/next.
    // Trả về true nếu gỡ được; giá trị đã gỡ ghi vào *removed (nếu khác nullptr). O(n).
    // BookingService (C) dùng hàm này khi cấp ghế cho khách ở giữa hàng.
    bool removeFirstIf(const std::function<bool(const T&)>& pred, T* removed = nullptr) {
        (void)pred;
        (void)removed;
        // TODO: duyệt từ head_. Khi gặp nút thỏa điều kiện:
        //   - có prev thì prev->next = cur->next, ngược lại head_ = cur->next
        //   - có next thì next->prev = cur->prev, ngược lại tail_ = cur->prev
        //   - delete cur, size_--
        todo("removeFirstIf");
    }

    // Các phần tử từ head đến tail (để hiển thị hàng chờ và lưu file). O(n).
    std::vector<T> toVector() const {
        // TODO
        todo("toVector");
    }

    void clear() {
        // TODO: duyệt và delete từng nút, đưa về trạng thái rỗng.
        //       Không ném lỗi vì destructor gọi hàm này.
    }
};
