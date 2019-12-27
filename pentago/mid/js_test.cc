// Test functions for wasm / js interface

#include "pentago/mid/midengine.h"
#include "pentago/utility/wasm.h"
#include <cstdint>
namespace pentago {

// Learn about alignment
static_assert(alignof(int) == 4);
static_assert(alignof(uint64_t) == 8);
static_assert(alignof(tuple<high_board_t,int>) == 4);
static_assert(alignof(midsolve_results_t) == 4);

// Learn about sizes
static_assert(sizeof(int) == 4);
static_assert(sizeof(uint64_t) == 8);
static_assert(sizeof(tuple<high_board_t,int>) == 12);
static_assert(sizeof(midsolve_results_t) == 4 + 12 * midsolve_results_t::limit);

WASM_EXPORT int sqr_test(const int n) {
  return n * n;
}

// High 32 bits of sum of 64 bit numbers
WASM_EXPORT uint32_t sum_test(const int num, const uint64_t* data) {
  uint64_t sum = 0;
  for (const auto n : RawArray<const uint64_t>(num, data))
    sum += n;
  return sum >> 32;
}

WASM_EXPORT void die_test() {
  die("An informative message");
}

}  // namespace pentago
