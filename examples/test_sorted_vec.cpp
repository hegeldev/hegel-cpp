#include <hegel/hegel.hpp>
#include <iostream>
#include <set>
#include <variant>

#include "sorted_vec.hpp"

using namespace hegel::st;

struct Insert {
  int value;
};
struct Remove {
  int value;
};
struct Contains {
  int value;
};
using Op = std::variant<Insert, Remove, Contains>;

int main() {
  // Generate operations
  auto value_gen = integers<int>({.min_value = -50, .max_value = 50});

  auto ops_gen =
      vectors(variant_(value_gen.map([](int v) { return Insert{v}; }),
                       value_gen.map([](int v) { return Remove{v}; }),
                       value_gen.map([](int v) { return Contains{v}; })),
              {.min_size = 1, .max_size = 30});
  auto ops = ops_gen.generate();
  std::cerr << "operations: " << ops.size() << std::endl;

  // Model: use std::multiset
  std::multiset<int> model;
  SortedVec<int> sv;

  for (const auto& op : ops) {
    if (std::holds_alternative<Insert>(op)) {
      int val = std::get<Insert>(op).value;
      std::cerr << "Insert(" << val << ")" << std::endl;
      model.insert(val);
      sv.insert(val);

      // Property: should remain sorted after insert
      if (!sv.is_sorted()) {
        std::cerr << "SortedVec not sorted after inserting " << val
                  << std::endl;
        return 1;
      }

    } else if (std::holds_alternative<Remove>(op)) {
      int val = std::get<Remove>(op).value;
      std::cerr << "Remove(" << val << ")" << std::endl;

      auto model_it = model.find(val);
      bool model_had = (model_it != model.end());
      if (model_had) {
        model.erase(model_it);  // Erase only one copy
      }

      bool sv_removed = sv.remove(val);

      // Property: remove should agree with model
      if (model_had != sv_removed) {
        std::cerr << "Remove disagreement for " << val
                  << ": model_had=" << model_had << " sv_removed=" << sv_removed
                  << std::endl;
        return 1;
      }

    } else {
      int val = std::get<Contains>(op).value;
      std::cerr << "Contains(" << val << ")" << std::endl;

      bool model_has = (model.find(val) != model.end());
      bool sv_has = sv.contains(val);

      // Property: contains should agree with model
      if (model_has != sv_has) {
        std::cerr << "Contains disagreement for " << val
                  << ": model=" << model_has << " sv=" << sv_has << std::endl;
        std::cerr << "SortedVec contents: ";
        for (size_t i = 0; i < sv.size(); ++i) {
          std::cerr << sv[i] << " ";
        }
        std::cerr << std::endl;
        return 1;
      }
    }

    // Invariant: sizes should match
    if (sv.size() != model.size()) {
      std::cerr << "Size mismatch: sv=" << sv.size()
                << " model=" << model.size() << std::endl;
      return 1;
    }
  }

  return 0;
}
