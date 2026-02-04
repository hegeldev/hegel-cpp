#include <hegel/hegel.h>
#include <iostream>
#include <map>
#include <set>
#include <variant>

#include "lru_cache.h"

using namespace hegel::st;

struct Put {
  int key;
  int value;
};
struct Get {
  int key;
};
using Op = std::variant<Put, Get>;

int main() {
  // Small capacity to trigger evictions
  auto cap = integers<size_t>({.min_value = 2, .max_value = 4}).generate();
  std::cerr << "capacity: " << cap << std::endl;

  // Generate operations with small key space to ensure collisions
  auto ops_gen = vectors(
      variant_(tuples(integers<int>({.min_value = 0, .max_value = 6}),
                      integers<int>({.min_value = 0, .max_value = 100}))
                   .map([](auto t) { return Put{std::get<0>(t), std::get<1>(t)}; }),
               integers<int>({.min_value = 0, .max_value = 6}).map([](int k) {
                 return Get{k};
               })),
      {.min_size = 5, .max_size = 30});
  auto ops = ops_gen.generate();
  std::cerr << "operations: " << ops.size() << std::endl;

  // Model: track what should be in cache and access order
  // We use a map for values and a list for LRU order
  std::map<int, int> model_data;
  std::list<int> model_order;  // front = most recent

  auto model_touch = [&](int key) {
    model_order.remove(key);
    model_order.push_front(key);
  };

  auto model_evict_lru = [&]() {
    if (!model_order.empty()) {
      int lru = model_order.back();
      model_order.pop_back();
      model_data.erase(lru);
    }
  };

  LRUCache<int, int> cache(cap);

  for (const auto& op : ops) {
    if (std::holds_alternative<Put>(op)) {
      auto [key, value] = std::get<Put>(op);
      std::cerr << "Put(" << key << ", " << value << ")" << std::endl;

      // Update model
      if (model_data.find(key) != model_data.end()) {
        // Update existing
        model_data[key] = value;
        model_touch(key);
      } else {
        // Insert new, possibly evicting
        if (model_data.size() >= cap) {
          model_evict_lru();
        }
        model_data[key] = value;
        model_touch(key);
      }

      // Update cache
      cache.put(key, value);

    } else {
      int key = std::get<Get>(op).key;
      std::cerr << "Get(" << key << ")" << std::endl;

      auto cache_result = cache.get(key);
      auto model_it = model_data.find(key);

      if (model_it == model_data.end()) {
        // Key shouldn't exist
        if (cache_result.has_value()) {
          std::cerr << "Cache returned value for non-existent key " << key << std::endl;
          return 1;
        }
      } else {
        // Key should exist with correct value
        if (!cache_result.has_value()) {
          std::cerr << "Cache missing key " << key << " that should exist" << std::endl;
          return 1;
        }
        if (*cache_result != model_it->second) {
          std::cerr << "Cache returned wrong value for key " << key << ": expected "
                    << model_it->second << " got " << *cache_result << std::endl;
          return 1;
        }
        // Mark as recently used in model
        model_touch(key);
      }
    }

    // Invariant: cache and model should have same keys
    if (cache.size() != model_data.size()) {
      std::cerr << "Size mismatch: cache=" << cache.size()
                << " model=" << model_data.size() << std::endl;
      return 1;
    }

    for (const auto& [k, v] : model_data) {
      if (!cache.contains(k)) {
        std::cerr << "Cache missing key " << k << " that model has" << std::endl;
        return 1;
      }
    }
  }

  return 0;
}
