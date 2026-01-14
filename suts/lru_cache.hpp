#pragma once

#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>

// An LRU cache with a bug: get() doesn't update access order
template <typename K, typename V> class LRUCache {
public:
  explicit LRUCache(size_t capacity) : capacity_(capacity) {
    if (capacity == 0) {
      throw std::invalid_argument("capacity must be positive");
    }
  }

  void put(const K &key, const V &value) {
    auto it = map_.find(key);
    if (it != map_.end()) {
      // Update existing: move to front and update value
      order_.erase(it->second.order_it);
      order_.push_front(key);
      it->second = {value, order_.begin()};
    } else {
      // Insert new
      if (map_.size() >= capacity_) {
        // Evict least recently used (back of list)
        K lru_key = order_.back();
        order_.pop_back();
        map_.erase(lru_key);
      }
      order_.push_front(key);
      map_[key] = {value, order_.begin()};
    }
  }

  std::optional<V> get(const K &key) {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return std::nullopt;
    }

    // BUG: Should move key to front of order_ list to mark as recently used
    // Missing:
    //   order_.erase(it->second.order_it);
    //   order_.push_front(key);
    //   it->second.order_it = order_.begin();

    return it->second.value;
  }

  bool contains(const K &key) const { return map_.find(key) != map_.end(); }

  size_t size() const { return map_.size(); }
  size_t capacity() const { return capacity_; }

private:
  struct Entry {
    V value;
    typename std::list<K>::iterator order_it;
  };

  size_t capacity_;
  std::list<K> order_; // front = most recent, back = least recent
  std::unordered_map<K, Entry> map_;
};
