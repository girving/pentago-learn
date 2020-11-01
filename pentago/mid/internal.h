// Internal routines for midengine.cc and metal equivalent
#pragma once

#include "pentago/mid/internal_c.h"
#include "pentago/mid/midengine_c.h"
#include "pentago/mid/halfsuper.h"
#include "pentago/mid/subsets.h"
#include "pentago/utility/integer_log.h"
NAMESPACE_PENTAGO

static inline grab_t make_grab(const bool end, const int nx, const int ny, const int workspace_size) {
  grab_t g;
  g.ny = ny;
  g.lo = end ? workspace_size - nx * ny : 0;
  return g;
}

// RawArray<halfsupers_t,2>, but minimal for Metal friendliness
struct io_t {
  METAL_DEVICE halfsupers_t* data;
  int stride;

  METAL_DEVICE halfsupers_t& operator()(const int i, const int j) const { return data[i * stride + j]; }
};

static inline io_t slice(METAL_DEVICE halfsupers_t* workspace, const grab_t g) {
  return io_t{workspace + g.lo, g.ny};
}

static inline info_t make_info(const high_board_t root, const int n, const int workspace_size) {
  info_t I;
  I.root = root.s;
  I.n = n;
  I.parity = root.middle();
  I.slice = root.count();
  I.spots = 36 - I.slice;
  I.k0 = (I.slice+n)/2 - (I.slice+(n&1))/2;
  I.k1 = n - I.k0;
  I.done = I.spots == I.n;
  I.root0 = root.side((I.slice+n)&1);
  I.root1 = root.side((I.slice+n+1)&1);
  I.sets0 = make_sets(I.spots, I.k0);
  I.sets1 = make_sets(I.spots, I.k1);
  I.sets1p = make_sets(I.spots-I.k0, I.k1);
  I.sets0_next = I.done ? make_sets(0, 1) : make_sets(I.spots, I.k0+1);
  I.cs1ps_size = I.sets1p.size * (I.spots-I.k0);
  I.empty = make_empty(root);
  I.input = make_grab(n&1, choose(I.spots, I.k0+1), choose(I.spots-I.k0-1, I.k1), workspace_size);
  I.output = make_grab(!(n&1), I.sets1.size, choose(I.spots-I.k1, I.k0), workspace_size);
  return I;
}

static inline wins_info_t make_wins_info(METAL_CONSTANT const info_t& I) {
  wins_info_t W;
  W.empty = I.empty;
  W.root0 = I.root0;
  W.root1 = I.root1;
  W.sets1 = I.sets1;
  W.sets0_next = I.sets0_next;
  W.size = W.sets1.size + W.sets0_next.size;
  W.parity = (I.n+I.parity)&1;
  return W;
}

// [:sets1.size] = all_wins1 = whether the other side wins for all possible sides
// [sets1.size:] = all_wins0_next = whether we win on the next move
static inline halfsuper_t mid_wins(METAL_CONSTANT const wins_info_t& W, const int s) {
  const bool one = s < W.sets1.size;
  const auto ss = one ? s : s - W.sets1.size;
  const auto root = one ? W.root1 : W.root0;
  const auto sets = one ? W.sets1 : W.sets0_next;
  return halfsuper_wins(root | side(W.empty, sets, ss), W.parity);
}

static inline uint16_t make_cs1ps(METAL_CONSTANT const info_t& I, METAL_DEVICE const set_t* sets1p, const int index) {
  const int s1p = index / (I.spots-I.k0);
  const int j = index - s1p * (I.spots-I.k0);
  uint16_t c = s1p;
  for (int a = 0; a < I.k1; a++) {
    const int s1p_a = sets1p[s1p]>>5*a&0x1f;
    if (j<s1p_a)
      c += fast_choose(s1p_a-1, a+1) - fast_choose(s1p_a, a+1);
  }
  return c;
}

static inline set0_info_t make_set0_info(METAL_CONSTANT const info_t& I, METAL_DEVICE const halfsuper_t* all_wins,
                                         const int s0) {
  set0_info_t I0;
  const int k0 = I.sets0.k;
  const int k1 = I.n - I.k0;

  // Construct side to move
  const set_t set0 = get(I.sets0, s0);
  const side_t side0 = I.root0 | side(I.empty, I.sets0, set0);

  // Evaluate whether we win for side0 and all child sides
  I0.wins0 = halfsuper_wins(side0, (I.n+I.parity)&1).s;

  // List empty spots after we place s0's stones
  const auto empty1 = I0.empty1;
  {
    const auto free = side_mask & ~side0;
    int next = 0;
    for (int i = 0; i < I.spots; i++)
      if (free&side_t(1)<<I.empty.empty[i])
        empty1[next++] = i;
  }

  /* What happens to indices when we add a stone?  We start with
   *   s0 = sum_i choose(set0[i],i+1)
   * at q with set0[j-1] < q < set0[j], the new index is
   *   cs0 = s0 + choose(q,j+1) + sum_{j<=i<k0} [ choose(set0[i],i+2)-choose(set0[i],i+1) ]
   *
   * What if we instead add a first white stone at empty1[q0] with set0[j0-1] < empty1[q0] < set0[j0]>
   * The new index is s0p_1:
   *   s0p_0 = s0
   *   s0p_1 = s0p_0 + sum_{j0<=i<k0} choose(set0[i]-1,i+1)-choose(set0[i],i+1)
   *         = s0p_0 + offset0[0][q0]
   * That is, all combinations from j0 on are shifted downwards by 1, and the total offset is precomputable.
   * If we add another stone at q1,j1, the shift is
   *   s0p_2 = s0p_1 + sum_{j1<=i<k0} choose(set0[i]-2,i+1)-choose(set0[i]-1,i+1)
   *         = s0p_1 + offset0[1][q1]
   * where we have used the fact that q0 < q1.  In general, we have
   *   s0p_a = s0p_{a-1} + sum_{ja<=i<k0} choose(set0[i]-a-1,i+1)-choose(set0[i]-a,i+1)
   *         = s0p_{a-1} + offset0[a][qa]
   *
   * So far this offset0 array is two dimensional, but now we have to take into account the new black
   * stones.  We must also be able to know where they go.  The easy thing is to make one 2D offset0 array
   * for each place we can put the black stone, but that requires us to do independent sums for every
   * possible move.  It seems difficult to beat, though, since the terms in the sum that makes up cs0p-s0p
   * can change independently as we add white stones.
   *
   * Overall, this precomputation seems quite complicated for an uncertain benefit, so I'll put it aside for now.
   *
   * ----------------------
   *
   * What if we start with s1p and add one black stone at empty1[i] to reach cs1p?  We get
   *
   *    s1p = sum_j choose(set1p[j],j+1)
   *   cs1p = sum_j choose(set1p[j]-(set1p[j]>i),j+1)
   *   cs1p = s1p + sum_{j s.t. set1p[j]>i} choose(set1p[j]-1,j+1) - choose(set1p[j],j+1)
   *
   * Ooh: that's complicated, but it doesn't depend at all on s0, so we can hoist the entire
   * thing.  See cs1ps above.
   */

  // Precompute absolute indices after we place s0's stones
  for (int i = 0; i < I.spots-I.k0; i++) {
    const int j = empty1[i]-i;
    I0.child_s0s[i] = choose(empty1[i], j+1);
    for (int a = 0; a < k0; a++)
      I0.child_s0s[i] += choose(set0>>5*a&0x1f, a+(a>=j)+1);
  }

  // Lookup table to convert s0 to s0p
  for (int a = 0; a < k1; a++) {
    for (int q = 0; q < I.spots-I.k0; q++) {
      I0.offset0[a][q] = a ? 0 : s0;
      for (int i = empty1[q]-q; i < I.k0; i++) {
        const int v = set0>>5*i&0x1f;
        if (v>a)
          I0.offset0[a][q] += fast_choose(v-a-1, i+1) - fast_choose(v-a, i+1);
      }
    }
  }
  return I0;
}

template<class Workspace> static inline void
inner(METAL_CONSTANT const info_t& I, METAL_DEVICE const uint16_t* cs1ps, METAL_DEVICE const set_t* sets1p,
      METAL_DEVICE const halfsuper_t* all_wins, METAL_DEVICE halfsupers_t* results, const Workspace workspace,
      METAL_DEVICE const set0_info_t& I0, const int s1p) {
  const auto set1p = sets1p[s1p];
  const auto input = slice(workspace, I.input);
  const auto output = slice(workspace, I.output);
  const auto all_wins1 = all_wins;  // all_wins1 is a prefix of all_wins
  const auto all_wins0_next = all_wins + I.sets1.size;

  // Convert indices
  uint32_t filled1p = 0;
  uint16_t s1 = 0;
  uint16_t s0p = 0;
  for (int i = 0; i < I.k1; i++) {
    const int q = set1p>>5*i&0x1f;
    filled1p |= 1<<q;
    s1 += fast_choose(I0.empty1[q], i+1);  // I0.offset1[i][q];
    s0p += I0.offset0[i][q];
  }

  // Consider each move in turn
  halfsupers_t us;
  {
    us.win = us.notlose = halfsuper_t(0);
    if (I.slice + I.n == 36)
      us.notlose = ~halfsuper_t(0);
    auto unmoved = ~filled1p;
    for (int m = 0; m < I.spots-I.k0-I.k1; m++) {
      const auto bit = min_bit(unmoved);
      const int i = integer_log_exact(bit);
      unmoved &= ~bit;
      const int cs0 = I0.child_s0s[i];
      const halfsuper_t cwins = all_wins0_next[cs0];
      const halfsupers_t child = input(cs0, cs1ps[s1p*(I.spots-I.k0)+i]);
      us.win = halfsuper_t(us.win) | cwins | child.win;
      us.notlose = halfsuper_t(us.notlose) | cwins | child.notlose;
    }
  }

  // Account for immediate results
  const halfsuper_t wins0(I0.wins0);
  const auto wins1 = all_wins1[s1];
  const auto inplay = ~(wins0 | wins1);
  us.win = (inplay & us.win) | (wins0 & ~wins1);  // win (| ~inplay & (wins0 & ~wins1))
  us.notlose = (inplay & us.notlose) | wins0;  // not lose       (| ~inplay & (wins0 | ~wins1))

  // If we're far enough along, remember results
  if (I.n <= 1)
    results[I.n + s1p] = us;

  // Negate and apply rmax in preparation for the slice above
  halfsupers_t above;
  above.win = rmax(~halfsuper_t(us.notlose));
  above.notlose = rmax(~halfsuper_t(us.win));
  output(s1,s0p) = above;
}

END_NAMESPACE_PENTAGO