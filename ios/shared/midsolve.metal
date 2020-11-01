// Midgame solver in metal

#include <metal_stdlib>
#include "pentago/mid/subsets.h"
#include "pentago/mid/internal.h"
#include "pentago/base/gen/halfsuper_wins.h"
using namespace metal;
using namespace pentago;

kernel void subsets(constant sets_t* sets [[buffer(0)]],
                    device set_t* output [[buffer(1)]],
                    const uint s [[thread_position_in_grid]]) {
  if (s >= uint(sets->size)) return;
  output[s] = get(*sets, s);
}

kernel void wins(constant wins_info_t* W [[buffer(0)]],
                 device halfsuper_t* all_wins [[buffer(1)]],
                 const uint s [[thread_position_in_grid]]) {
  if (s >= uint(W->size)) return;
  all_wins[s] = mid_wins(*W, s);
}

kernel void cs1ps(constant info_t* I [[buffer(0)]],
                  const device set_t* sets1p [[buffer(1)]],
                  device uint16_t* cs1ps [[buffer(2)]],
                  const uint i [[thread_position_in_grid]]) {
  if (i >= uint(I->cs1ps_size)) return;
  cs1ps[i] = make_cs1ps(*I, sets1p, i);
}

kernel void set0_info(constant info_t* I [[buffer(0)]],
                      const device halfsuper_t* all_wins [[buffer(1)]],
                      device set0_info_t* I0 [[buffer(2)]],
                      const uint s0 [[thread_position_in_grid]]) {
  if (s0 >= uint(I->sets0.size)) return;
  I0[s0] = make_set0_info(*I, all_wins, s0);
}

// iOS has an unfortunate 256 MB buffer size limit, so we need to split workspace into up to four buffers
constant int chunk_size = (uint64_t(256) << 20) / sizeof(halfsupers_t);
constant int chunk_bits = 23;
static_assert(chunk_size == 1 << chunk_bits, "");

struct workspace_t {
  METAL_DEVICE halfsupers_t* chunks[4];
};

struct workspace_io_t {
  workspace_t w;
  int offset, stride;
  
  METAL_DEVICE halfsupers_t& operator()(const int i, const int j) const {
    const int r = i * stride + j + offset;
    return w.chunks[r >> chunk_bits][r & (chunk_size - 1)];
  }
};

workspace_io_t slice(const workspace_t w, const grab_t g) {
  return workspace_io_t{w, g.lo, g.ny};
}

kernel void inner(constant info_t* I [[buffer(0)]],
                  const device uint16_t* cs1ps [[buffer(1)]],
                  const device set_t* sets1p [[buffer(2)]],
                  const device halfsuper_t* all_wins [[buffer(3)]],
                  const device set0_info_t* I0 [[buffer(4)]],
                  device mid_super_t* results [[buffer(5)]],
                  device halfsupers_t* workspace0 [[buffer(6)]],
                  device halfsupers_t* workspace1 [[buffer(7)]],
                  device halfsupers_t* workspace2 [[buffer(8)]],
                  device halfsupers_t* workspace3 [[buffer(9)]],
                  const uint s [[thread_position_in_grid]]) {
  if (s >= uint(I->sets0.size * I->sets1p.size)) return;
  const auto s0 = s / I->sets1p.size;
  const auto s1p = s - s0 * I->sets1p.size;
  const workspace_t w{{workspace0, workspace1, workspace2, workspace3}};
  pentago::inner(*I, cs1ps, sets1p, all_wins, results, w, I0[s0], s1p);
}