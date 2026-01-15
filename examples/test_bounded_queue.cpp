#include <hegel/hegel.hpp>
#include <iostream>
#include <variant>

#include "bounded_queue.hpp"

using namespace hegel::st;

struct Push {
  int value;
};
struct Pop {};
using Op = std::variant<Push, Pop>;

int main() {
  // Generate queue capacity (small to find bugs faster)
  auto cap = integers<size_t>({.min_value = 1, .max_value = 5}).generate();
  std::cerr << "capacity: " << cap << std::endl;

  // Generate a sequence of operations
  auto ops_gen = vectors(
      variant_(
          integers<int>({.min_value = -100, .max_value = 100}).map([](int v) {
            return Push{v};
          }),
          just(Pop{})),
      {.min_size = 1, .max_size = 20});
  auto ops = ops_gen.generate();
  std::cerr << "operations: " << ops.size() << std::endl;

  // Model: use a simple vector to track expected state
  std::vector<int> model;
  BoundedQueue<int> queue(cap);

  for (const auto& op : ops) {
    if (std::holds_alternative<Push>(op)) {
      int val = std::get<Push>(op).value;
      std::cerr << "Push(" << val << ")" << std::endl;

      bool model_accepted = model.size() < cap;
      bool queue_accepted = queue.push(val);

      if (model_accepted) {
        model.push_back(val);
      }

      // Property: queue and model should agree on whether push succeeded
      if (model_accepted != queue_accepted) {
        std::cerr << "Push disagreement: model=" << model_accepted
                  << " queue=" << queue_accepted << " size=" << queue.size()
                  << " cap=" << cap << std::endl;
        return 1;
      }
    } else {
      std::cerr << "Pop()" << std::endl;
      auto queue_result = queue.pop();

      if (model.empty()) {
        // Property: pop on empty should return nullopt
        if (queue_result.has_value()) {
          std::cerr << "Pop from empty queue returned value" << std::endl;
          return 1;
        }
      } else {
        // Property: pop should return the front element
        if (!queue_result.has_value()) {
          std::cerr << "Pop from non-empty queue returned nullopt" << std::endl;
          return 1;
        }
        if (*queue_result != model.front()) {
          std::cerr << "Pop returned " << *queue_result << " but expected "
                    << model.front() << std::endl;
          return 1;
        }
        model.erase(model.begin());
      }
    }

    // Invariant: sizes should match
    if (queue.size() != model.size()) {
      std::cerr << "Size mismatch: queue=" << queue.size()
                << " model=" << model.size() << std::endl;
      return 1;
    }
  }

  return 0;
}
