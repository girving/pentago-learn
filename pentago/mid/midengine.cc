// Solve positions near the end of the game using downstream retrograde analysis

#include "pentago/mid/midengine.h"
#include "pentago/mid/subsets.h"
#include "pentago/base/symmetry.h"
#include "pentago/utility/ceil_div.h"
#include "pentago/utility/debug.h"
#include "pentago/utility/wasm_alloc.h"
#ifndef __wasm__
#include "pentago/utility/aligned.h"
#include "pentago/utility/log.h"
#endif  // !__wasm__
NAMESPACE_PENTAGO

using std::max;
using std::make_tuple;

/* In the code below, we will organized pairs of subsets of [0,n-1] into two dimensional arrays.
 * The first dimension records the player to move, the second the other player.  The first dimension
 * is organized according to the order produced by subsets(), and the second is organized by the
 * order produced by subsets *relative* to the first set (a sort of semi-direct product).  For example,
 * if our root position has slice 32, or 4 empty spots, the layout is
 *
 *   n 0 : ____
 *   n 1 : 0___  _0__  __0_ ___0
 *
 *   n 2 : 01__  0_1_  0__1
 *         10__  _01_  _0_1
 *         1_0_  _10_  __01
 *         1__0  _1_0  __10
 *
 *   n 3 : 100_  10_0  1_00
 *         010_  01_0  _100
 *         001_  0_10  _010
 *         00_1  0_01  _001
 *
 *   n 4 : 0011
 *         0101
 *         1001
 *         0110
 *         1010
 *         1100
 */

static int bottleneck(const int spots) {
  int worst = 0;
  int prev = 1;
  for (const int n : range(spots+1)) {
    int next = choose(spots, n+1);
    if (next) next *= choose(n+1, (n+1)/2);
    worst = max(worst, prev + next);
    prev = next;
  }
  return worst;
}

int midsolve_workspace_size(const int min_slice) {
  return bottleneck(36 - min_slice);
}

#ifndef __wasm__
Array<halfsupers_t> midsolve_workspace(const int min_slice) {
  return aligned_buffer<halfsupers_t>(midsolve_workspace_size(min_slice));
}
#endif  // !__wasm__

static RawArray<halfsupers_t,2> grab(RawArray<halfsupers_t> workspace, const bool end,
                                     const int nx, const int ny) {
  const auto flat = end ? workspace.slice(workspace.size()-nx*ny, workspace.size()) : workspace.slice(0,nx*ny);
  return flat.reshape(vec(nx, ny));
}

static void midsolve_loop(const high_board_t root, const int n, mid_supers_t& results,
                          RawArray<halfsupers_t> workspace) {
  const int parity = root.middle();
  const int slice = root.count();
  const int spots = 36 - slice;
  const bool done = spots == n;
  const auto root0 = root.side((slice+n)&1),
             root1 = root.side((slice+n+1)&1);
  const int k0 = (slice+n)/2 - (slice+(n&1))/2,  // Player to move
            k1 = n-k0;  // Other player
  const auto sets0 = sets_t(spots, k0);
  const auto sets1 = sets_t(spots, k1);
  ALLOCA_SUBSETS(sets1p, spots-k0, k1);
  const auto input = grab(workspace, n&1, choose(spots, k0+1), choose(spots-k0-1, k1)).const_();
  const auto output = grab(workspace, !(n&1), sets1.size, choose(spots-k1, k0));
  const empty_t empty(root);

  // Evaluate whether the other side wins for all possible sides
  halfsuper_t all_wins1[sets1.size];
  for (const int s1 : range(sets1.size))
    all_wins1[s1] = halfsuper_wins(root1 | empty.side(sets1, s1), (n+parity)&1);

  // Precompute whether we win on the next move
  halfsuper_t all_wins0_next[done ? 0 : choose(spots, k0+1)];
  if (!done) {
    const auto sets0_next = sets_t(spots, k0+1);
    for (int s0 = 0; s0 < sets0_next.size; s0++)
      all_wins0_next[s0] = halfsuper_wins(root0 | empty.side(sets0_next, s0), (n+parity)&1);
  }

  // Lookup table for converting s1p to cs1p (s1 relative to one more black stone):
  //   cs1p = cs1ps[s1p].x[j] if we place a black stone at empty1[j]
  uint16_t cs1ps[sets1p.size()][spots-k0];
  for (int s1p = 0; s1p < sets1p.size(); s1p++) {
    for (int j = 0; j < spots-k0; j++) {
      cs1ps[s1p][j] = s1p;
      for (int a = 0; a < k1; a++) {
        const int s1p_a = sets1p[s1p]>>5*a&0x1f;
        if (j<s1p_a)
          cs1ps[s1p][j] += fast_choose(s1p_a-1, a+1) - fast_choose(s1p_a, a+1);
      }
    }
  }

  // Iterate over set of stones of player to move
  for (const int s0 : range(sets0.size)) {
    // Construct side to move
    const set_t set0 = sets0(s0);
    const side_t side0 = root0 | empty.side(sets0, set0);

    // Make a mask of filled spots
    uint32_t filled0 = 0;
    for (int i = 0; i < k0; i++)
      filled0 |= 1<<(set0>>5*i&0x1f);

    // Evaluate whether we win for side0 and all child sides
    const halfsuper_t wins0 = halfsuper_wins(side0, (n+parity)&1);

    // List empty spots after we place s0's stones
    uint8_t empty1[spots-k0];
    {
      const auto free = side_mask & ~side0;
      int next = 0;
      for (int i = 0; i < spots; i++)
        if (free&side_t(1)<<empty.empty[i])
          empty1[next++] = i;
      NON_WASM_ASSERT(next==spots-k0);
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
    uint16_t child_s0s[spots-k0];
    for (int i = 0; i < spots-k0; i++) {
      const int j = empty1[i]-i;
      child_s0s[i] = choose(empty1[i], j+1);
      for (int a = 0; a < k0; a++)
        child_s0s[i] += choose(set0>>5*a&0x1f, a+(a>=j)+1);
    }

    // Preload wins after we place s0's stones.
    halfsuper_t child_wins0[spots-k0];
    if (!done)
      for (int i = 0; i < spots-k0; i++)
        child_wins0[i] = all_wins0_next[child_s0s[i]];

    // Lookup table to convert s1p to s1
    uint16_t offset1[k1][spots-k0];
    for (int i = 0; i < k1; i++)
      for (int q = 0; q < spots-k0; q++)
        offset1[i][q] = fast_choose(empty1[q], i+1);

    // Lookup table to convert s0 to s0p
    uint16_t offset0[k1][spots-k0];
    for (int a = 0; a < k1; a++)
      for (int q = 0; q < spots-k0; q++) {
        offset0[a][q] = 0;
        for (int i = empty1[q]-q; i < k0; i++) {
          const int v = set0>>5*i&0x1f;
          if (v>a)
            offset0[a][q] += fast_choose(v-a-1, i+1) - fast_choose(v-a, i+1);
        }
      }

    // Iterate over set of stones of other player
    for (int s1p = 0; s1p < sets1p.size(); s1p++) {
      const auto set1p = sets1p[s1p];

      // Convert indices
      uint32_t filled1 = 0;
      uint32_t filled1p = 0;
      uint16_t s1 = 0;
      uint16_t s0p = s0;
      for (int i = 0; i < k1; i++) {
        const int q = set1p>>5*i&0x1f;
        filled1 |= 1<<empty1[q];
        filled1p |= 1<<q;
        s1  += offset1[i][q];
        s0p += offset0[i][q];
      }

      // Consider each move in turn
      halfsupers_t us;
      {
        us[0] = us[1] = halfsuper_t(0);
        if (slice + n == 36)
          us[1] = ~halfsuper_t(0);
        for (int i = 0; i < spots-k0; i++)
          if (!(filled1p&1<<i)) {
            const int cs0 = child_s0s[i];
            const auto cwins = child_wins0[i];
            const halfsupers_t child = input(cs0,cs1ps[s1p][i]);
            us[0] |= cwins | child[0];  // win
            us[1] |= cwins | child[1];  // not lose
          }
      }

      // Account for immediate results
      const halfsuper_t wins1 = all_wins1[s1],
                        inplay = ~(wins0 | wins1);
      us[0] = (inplay & us[0]) | (wins0 & ~wins1);  // win (| ~inplay & (wins0 & ~wins1))
      us[1] = (inplay & us[1]) | wins0;  // not lose       (| ~inplay & (wins0 | ~wins1))

      // If we're far enough along, remember results
      if (n <= 1) {
        const uint32_t filled_black = (slice+n)&1 ? filled1 : filled0,
                       filled_white = (slice+n)&1 ? filled0 : filled1;
        auto sides = root.sides();
        for (int i = 0; i < spots; i++) {
          sides[0] |= side_t(filled_black>>i&1) << empty.empty[i];
          sides[1] |= side_t(filled_white>>i&1) << empty.empty[i];
        }
        results.append(make_tuple(sides, superinfos_t{us[0], us[1], bool((n+parity)&1)}));
      }

      // Negate and apply rmax in preparation for the slice above
      halfsupers_t above;
      above[0] = rmax(~us[1]);
      above[1] = rmax(~us[0]);
      output(s1,s0p) = above;
    }
  }
}

mid_supers_t midsolve_internal(const high_board_t board, RawArray<halfsupers_t> workspace) {
  const int spots = 36 - board.count();
  NON_WASM_ASSERT(workspace.size() >= bottleneck(spots));

  // Compute all slices
  mid_supers_t results;
  for (int n = spots; n >= 0; n--)
    midsolve_loop(board, n, results, workspace);
  return results;
}

static int traverse(const high_board_t board, const mid_supers_t& supers, mid_values_t& results) {
  int value;
  const auto [done, immediate_value] = board.done_and_value();
  if (done) { // Done, so no lookup required
    value = immediate_value;
  } else if (!board.middle()) {  // Recurse into children
    value = -1;
    const auto empty = board.empty_mask();
    for (const int bit : range(64))
      if (empty & side_t(1)<<bit)
        value = max(value, traverse(board.place(bit), supers, results));
  } else {  // if board.middle()
    // Find unrotated board in supers
    int i;
    for (i = 0; i < supers.size(); i++)
      if (get<0>(supers[i]) == board.sides())
        break;
    NON_WASM_ASSERT(i < supers.size());
    const superinfos_t& r = get<1>(supers[i]);

    // Handle recursion manually to avoid actual rotation
    value = -1;
    for (const int s : range(8)) {
      const int q = s >> 1;
      const int d = s & 1 ? -1 : 1;
      const auto v = r.value((d & 3) << 2*q);
      results.append(make_tuple(board.rotate(q, d), v));
      value = max(value, -v);
    }
  }

  // Store and return value
  results.append(make_tuple(board, value));
  return value;
}

#if !defined(__wasm__) || defined(__APPLE__)
mid_values_t midsolve(const high_board_t board, RawArray<halfsupers_t> workspace) {
  // Compute
  const auto supers = midsolve_internal(board, workspace);

  // Extract all available boards
  mid_values_t results;
  traverse(board, supers, results);
  return results;
}
#else  // if __wasm__
WASM_EXPORT void midsolve(const high_board_t* board, mid_values_t* results) {
  NON_WASM_ASSERT(board && results);
  results->clear();
  const auto workspace = wasm_buffer<halfsupers_t>(midsolve_workspace_size(board->count()));
  const auto supers = midsolve_internal(*board, workspace);
  traverse(*board, supers, *results);
}
#endif  // __wasm__

END_NAMESPACE_PENTAGO
