#pragma once

#include <optional>
#include <stdexcept>
#include <vector>

// A bounded circular queue with a subtle off-by-one bug
template <typename T> class BoundedQueue {
  public:
    explicit BoundedQueue(size_t capacity)
        : buffer_(capacity), capacity_(capacity), head_(0), tail_(0), size_(0) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
    }

    bool push(const T& value) {
        if (is_full()) {
            return false;
        }
        buffer_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        return true;
    }

    std::optional<T> pop() {
        if (is_empty()) {
            return std::nullopt;
        }
        T value = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        --size_;
        return value;
    }

    std::optional<T> peek() const {
        if (is_empty()) {
            return std::nullopt;
        }
        return buffer_[head_];
    }

    bool is_empty() const { return size_ == 0; }

    // BUG: Should be size_ >= capacity_, but uses > instead
    // This allows pushing one extra element, corrupting the queue
    bool is_full() const { return size_ > capacity_; }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

  private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;
};
