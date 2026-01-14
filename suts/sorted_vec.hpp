#pragma once

#include <algorithm>
#include <vector>

// A vector that maintains sorted order, with a bug in binary search
template <typename T> class SortedVec {
public:
  void insert(const T &value) {
    auto pos = find_insert_pos(value);
    data_.insert(data_.begin() + pos, value);
  }

  bool contains(const T &value) const {
    if (data_.empty())
      return false;
    size_t idx = find_pos(value);
    return idx < data_.size() && data_[idx] == value;
  }

  bool remove(const T &value) {
    if (data_.empty())
      return false;
    size_t idx = find_pos(value);
    if (idx < data_.size() && data_[idx] == value) {
      data_.erase(data_.begin() + idx);
      return true;
    }
    return false;
  }

  size_t size() const { return data_.size(); }
  bool empty() const { return data_.empty(); }

  const T &operator[](size_t idx) const { return data_[idx]; }

  // Check if actually sorted (for testing)
  bool is_sorted() const {
    for (size_t i = 1; i < data_.size(); ++i) {
      if (data_[i] < data_[i - 1])
        return false;
    }
    return true;
  }

  const std::vector<T> &data() const { return data_; }

private:
  // Find position where value should be inserted
  size_t find_insert_pos(const T &value) const {
    if (data_.empty())
      return 0;

    size_t lo = 0;
    size_t hi = data_.size();

    while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2;
      if (data_[mid] < value) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    return lo;
  }

  // Find position of value (or where it would be)
  // BUG: Uses wrong comparison, causing off-by-one in some cases
  size_t find_pos(const T &value) const {
    if (data_.empty())
      return 0;

    size_t lo = 0;
    size_t hi = data_.size();

    while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2;
      // BUG: Should be data_[mid] < value, but uses <=
      // This causes it to skip past equal elements in some cases
      if (data_[mid] <= value) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    // BUG: Returns lo, but should return lo-1 or handle the <= case
    return lo > 0 ? lo - 1 : 0;
  }

  std::vector<T> data_;
};
