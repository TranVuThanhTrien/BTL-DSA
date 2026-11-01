// Deque (hàng đợi hai đầu) tự cài bằng danh sách liên kết đôi. KHÔNG dùng std::deque.
//
// Bất biến:
//   - rỗng:  head_ == nullptr, tail_ == nullptr, size_ == 0
//   - head_->prev == nullptr và tail_->next == nullptr
//   - với mọi nút n: n->next->prev == n và n->prev->next == n
//
// Độ phức tạp thời gian:
//   pushFront, pushBack, popFront, popBack, peekFront, peekBack, size, empty : O(1)
//   removeFirstIf, toVector, clear                                           : O(n)
// Độ phức tạp bộ nhớ: O(n), mỗi phần tử một nút cấp phát riêng.
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

    void requireNonEmpty() const {
        if (size_ == 0) throw std::underflow_error("Deque rong");
    }

public:
    Deque() = default;
    ~Deque() { clear(); }
    Deque(const Deque&) = delete;  // tránh hai Deque cùng giữ một chuỗi nút
    Deque& operator=(const Deque&) = delete;

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // Thêm vào đầu. O(1).
    void pushFront(const T& v) {
        Node* n = new Node(v);
        if (!head_) {
            head_ = tail_ = n;
        } else {
            n->next = head_;
            head_->prev = n;
            head_ = n;
        }
        ++size_;
    }

    // Thêm vào cuối. O(1).
    void pushBack(const T& v) {
        Node* n = new Node(v);
        if (!tail_) {
            head_ = tail_ = n;
        } else {
            n->prev = tail_;
            tail_->next = n;
            tail_ = n;
        }
        ++size_;
    }

    // Lấy ra và trả về phần tử đầu. Ném std::underflow_error nếu rỗng. O(1).
    T popFront() {
        requireNonEmpty();
        Node* n = head_;
        T value = n->data;
        head_ = n->next;
        if (head_) head_->prev = nullptr;
        else tail_ = nullptr;
        delete n;
        --size_;
        return value;
    }

    // Lấy ra và trả về phần tử cuối. Ném std::underflow_error nếu rỗng. O(1).
    T popBack() {
        requireNonEmpty();
        Node* n = tail_;
        T value = n->data;
        tail_ = n->prev;
        if (tail_) tail_->next = nullptr;
        else head_ = nullptr;
        delete n;
        --size_;
        return value;
    }

    // Xem phần tử đầu / cuối mà không gỡ. Ném std::underflow_error nếu rỗng. O(1).
    T& peekFront() { requireNonEmpty(); return head_->data; }
    T& peekBack() { requireNonEmpty(); return tail_->data; }

    // Gỡ phần tử đầu tiên (tính từ head_) thỏa điều kiện, nối lại prev/next.
    // Trả về true nếu gỡ được; giá trị đã gỡ ghi vào *removed (nếu khác nullptr). O(n).
    bool removeFirstIf(const std::function<bool(const T&)>& pred, T* removed = nullptr) {
        for (Node* cur = head_; cur; cur = cur->next) {
            if (!pred(cur->data)) continue;
            if (removed) *removed = cur->data;
            if (cur->prev) cur->prev->next = cur->next;
            else head_ = cur->next;
            if (cur->next) cur->next->prev = cur->prev;
            else tail_ = cur->prev;
            delete cur;
            --size_;
            return true;
        }
        return false;
    }

    // Các phần tử từ head đến tail. O(n).
    std::vector<T> toVector() const {
        std::vector<T> out;
        out.reserve(size_);
        for (Node* cur = head_; cur; cur = cur->next) out.push_back(cur->data);
        return out;
    }

    // Giải phóng toàn bộ nút. O(n). Không ném lỗi vì destructor gọi hàm này.
    void clear() {
        while (head_) {
            Node* n = head_;
            head_ = n->next;
            delete n;
        }
        tail_ = nullptr;
        size_ = 0;
    }
};


