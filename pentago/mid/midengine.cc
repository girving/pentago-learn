// Solve positions near the end of the game using downstream retrograde analysis

#include "pentago/mid/midengine.h"
#include "pentago/base/count.h"
#include "pentago/base/symmetry.h"
#include "pentago/high/board.h"
#include "pentago/search/superengine.h"
#include "pentago/utility/aligned.h"
#include "pentago/utility/ceil_div.h"
#include "pentago/utility/char_view.h"
#include "pentago/utility/debug.h"
#include "pentago/utility/memory.h"
#if PENTAGO_SSE
namespace pentago {

using std::max;
using std::make_pair;

// Flip to enable debugging
//#define MD(...) __VA_ARGS__
#define MD(...)

static superinfos_t transform_superinfos(const symmetry_t s, const superinfos_t a) {
  superinfos_t sa;
  sa.known   = transform_super(s,a.known);
  sa.win     = transform_super(s,a.win);
  sa.notlose = transform_super(s,a.notlose);
  return sa;
}

typedef uint64_t set_t;

template<int k> static inline void subsets_helper(vector<set_t>& all, const int n, const set_t high) {
  for (int i=k-1;i<n;i++)
    subsets_helper<k-1>(all,i,i|high<<5);
}
template<> inline void subsets_helper<0>(vector<set_t>& all, const int n, const set_t high) {
  all.push_back(high);
}

// All k-subsets of [0,n-1], packed into 64-bit ints with 5 bits for each entry.
// I.e., the set {a < b < c} has value a|b<<5|c<<10.  We order sets in large-entry-major order;
// the first 3-set of 10 is {0,1,2}, followed by {0,1,3}.
static Array<const set_t> subsets(const int n, const int k) {
  GEODE_ASSERT(0<=n && n<=18);
  vector<set_t> all;
  if (0<=k && k<=n)
    switch (k) {
      #define K(k) case k: subsets_helper<k>(all,n,0); break;
      K(0) K(1) K(2) K(3) K(4) K(5) K(6) K(7) K(8) K(9)
      #undef K
      default: GEODE_ASSERT(k<=9);
    }
  GEODE_ASSERT(uint64_t(all.size())==choose(n,k));
  return asarray(all).copy();
}

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

static int count(const int spots, const int more) {
  return CHECK_CAST_INT(choose(spots,more)*choose(more,more/2));
}

static int bottleneck(const int spots) {
  const int slice = 36-spots;
  int worst = count(spots,spots);
  for (const int n : range(spots+1)) {
    const int k0 = (slice+n)/2-(slice+(n&1))/2, // Player to move
              k1 = n-k0; // Other player
    worst = max(worst, CHECK_CAST_INT(count(spots,n)+count(spots,n+1)+ceil_div(choose(spots,k1),2)));
  }
  return worst;
}

Array<uint8_t> midsolve_workspace(const int min_slice) {
  // Allocate enough memory for 18 stones, which is the most we'll need
  return char_view_own(aligned_buffer<halfsupers_t>(bottleneck(36-min_slice)));
}

uint64_t midsolve_workspace_memory_usage(const int min_slice) {
  // Add a bit so that we can fix alignment if it's wrong
  return sizeof(halfsupers_t)*(bottleneck(36-min_slice)+2);
}

static RawArray<halfsupers_t,2> grab(RawArray<uint8_t> workspace, const bool end,
                                     const int nx, const int ny) {
  RawArray<halfsupers_t> work(workspace.size()/sizeof(halfsupers_t),(halfsupers_t*)workspace.data());
  const auto flat = end ? work.slice(work.size()-nx*ny, work.size()) : work.slice(0,nx*ny);
  return flat.reshape(vec(nx, ny));
}

static inline uint16_t fast_choose(const int n, const int k) {
  // Precompute some combinations, and store in a triangular array.  Generated by
  //   '{%s}'%','.join('{%s}'%','.join(str(choose(n,k)) for k in xrange(10+1)) for n in xrange(18+1))
  static const uint16_t combinations[18+1][10+1] = {{1,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0},{1,2,1,0,0,0,0,0,0,0,0},{1,3,3,1,0,0,0,0,0,0,0},{1,4,6,4,1,0,0,0,0,0,0},{1,5,10,10,5,1,0,0,0,0,0},{1,6,15,20,15,6,1,0,0,0,0},{1,7,21,35,35,21,7,1,0,0,0},{1,8,28,56,70,56,28,8,1,0,0},{1,9,36,84,126,126,84,36,9,1,0},{1,10,45,120,210,252,210,120,45,10,1},{1,11,55,165,330,462,462,330,165,55,11},{1,12,66,220,495,792,924,792,495,220,66},{1,13,78,286,715,1287,1716,1716,1287,715,286},{1,14,91,364,1001,2002,3003,3432,3003,2002,1001},{1,15,105,455,1365,3003,5005,6435,6435,5005,3003},{1,16,120,560,1820,4368,8008,11440,12870,11440,8008},{1,17,136,680,2380,6188,12376,19448,24310,24310,19448},{1,18,153,816,3060,8568,18564,31824,43758,48620,43758}};
  assert(unsigned(n)<=18 && unsigned(k)<=10);
  return combinations[n][k];
}

template<int spots,int n> __attribute__((noinline)) static void
midsolve_loop(const board_t root, const bool parity, unordered_map<board_t,superinfos_t>& results,
              RawArray<uint8_t> workspace) {
  const int slice = 36-spots;
  const auto black_root = unpack(root,0),
             white_root = unpack(root,1);
  const auto root0 = (slice+n)&1 ? white_root : black_root,
             root1 = (slice+n)&1 ? black_root : white_root;
  const int k0 = (slice+n)/2-(slice+(n&1))/2, // Player to move
            k1 = n-k0; // Other player
  const auto sets0 = subsets(spots,k0),
             sets0p = subsets(spots-k1,k0),
             sets1 = subsets(spots,k1),
             sets1p = subsets(spots-k0,k1);
  const int csets0_size = fast_choose(spots,k0+1),
            csets1p_size = spots-k0 ? fast_choose(spots-k0-1,k1) : 0;
  GEODE_ASSERT(!(uint64_t(workspace.data())&(sizeof(halfsupers_t)-1)));
  const auto input = grab(workspace,n&1,csets0_size,csets1p_size).const_();
  const auto output = grab(workspace,!(n&1),sets1.size(),sets0p.size());
  const int all_wins_start = CHECK_CAST_INT(memory_usage(n&1?output.const_():input));
  const RawArray<halfsuper_t> all_wins1(sets1.size(),(halfsuper_t*)(workspace.data()+all_wins_start));
  GEODE_ASSERT(memory_usage(input)+memory_usage(output)+memory_usage(all_wins1) <= uint64_t(workspace.size()));

  // List empty spots as bit indices into side_t
  uint8_t empty[max(spots, 1)] = {0};
  {
    const auto free = side_mask&~(black_root|white_root);
    int next = 0;
    for (int i=0;i<64;i++)
      if (free & side_t(1)<<i)
        empty[next++] = i;
    GEODE_ASSERT(next == spots);
  }
  #define set_side(count,set) ({ \
    const int c_ = (count); \
    const set_t set_ = (set); \
    side_t s_ = 0; \
    for (int i=0;i<c_;i++) \
      s_ |= side_t(1)<<empty[set_>>5*i&0x1f]; \
    s_; })
  MD(const auto str_set = [=](const int k0, const set_t set0, const int k1, const set_t set1, const bool flip) {
    string s(spots,'\0');
    for (int i=0;i<k0;i++) s[set0>>5*i&0x1f] += flip ? 2 : 1;
    for (int i=0;i<k1;i++) s[set1>>5*i&0x1f] += flip ? 1 : 2;
    for (int i=0;i<spots;i++)
      s[i] = uint8_t(s[i])<3 ? "_01"[int(s[i])] : 'x';
    return s;
  };)

  // Evaluate whether the other side wins for all possible sides
  for (int s1=0;s1<sets1.size();s1++)
    all_wins1[s1] = halfsuper_wins(root1 | set_side(k1,sets1[s1]))[(n+parity)&1];

  // Lookup table for converting s1p to cs1p (s1 relative to one more black stone:
  //   cs1p = cs1ps[s1p].x[j] if we place a black stone at empty1[j]
  Array<Vector<uint16_t,spots-k0>> cs1ps(sets1p.size(),uninit);
  for (int s1p=0;s1p<sets1p.size();s1p++) {
    for (int j=0;j<spots-k0;j++) {
      cs1ps[s1p][j] = s1p;
      for (int a=0;a<k1;a++) {
        const int s1p_a = sets1p[s1p]>>5*a&0x1f;
        if (j<s1p_a)
          cs1ps[s1p][j] += fast_choose(s1p_a-1,a+1)-fast_choose(s1p_a,a+1);
      }
    }
  }

  // Make sure each output entry is written only once
  MD(Array<uint8_t,2> written(output.sizes()));
 
  // Iterate over set of stones of player to move
  for (int s0=0;s0<sets0.size();s0++) {
    // Construct side to move
    const set_t set0 = sets0[s0];
    const side_t side0 = root0 | set_side(k0,set0);

    // Make a mask of filled spots
    uint32_t filled0 = 0;
    for (int i=0;i<k0;i++)
      filled0 |= 1<<(set0>>5*i&0x1f);

    // Evaluate whether we win for side0 and all child sides
    const halfsuper_t wins0 = halfsuper_wins(side0)[(n+parity)&1];

    // List empty spots after we place s0's stones
    uint8_t empty1[spots-k0];
    {
      const auto free = side_mask&~side0;
      int next = 0;
      for (int i=0;i<spots;i++)
        if (free&side_t(1)<<empty[i])
          empty1[next++] = i;
      GEODE_ASSERT(next==spots-k0);
    }

    // Precompute wins after we place s0's stones
    halfsuper_t child_wins0[spots-k0];
    for (int i=0;i<spots-k0;i++)
      child_wins0[i] = halfsuper_wins(side0|side_t(1)<<empty[empty1[i]])[(n+parity)&1];

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
    for (int i=0;i<spots-k0;i++) {
      const int j = empty1[i]-i;
      child_s0s[i] = choose(empty1[i],j+1);
      for (int a=0;a<k0;a++)
        child_s0s[i] += choose(set0>>5*a&0x1f,a+(a>=j)+1);
    }

    // Lookup table to convert s1p to s1
    uint16_t offset1[k1][spots-k0];
    for (int i=0;i<k1;i++)
      for (int q=0;q<spots-k0;q++)
        offset1[i][q] = fast_choose(empty1[q],i+1);

    // Lookup table to convert s0 to s0p
    uint16_t offset0[k1][spots-k0];
    for (int a=0;a<k1;a++)
      for (int q=0;q<spots-k0;q++) {
        offset0[a][q] = 0;
        for (int i=empty1[q]-q;i<k0;i++) {
          const int v = set0>>5*i&0x1f;
          if (v>a)
            offset0[a][q] += fast_choose(v-a-1,i+1)-fast_choose(v-a,i+1);
        }
      }

    // Iterate over set of stones of other player
    for (int s1p=0;s1p<sets1p.size();s1p++) {
      const auto set1p = sets1p[s1p];

      // Convert indices
      uint32_t filled1 = 0;
      uint32_t filled1p = 0;
      uint16_t s1 = 0;
      uint16_t s0p = s0;
      for (int i=0;i<k1;i++) {
        const int q = set1p>>5*i&0x1f;
        filled1 |= 1<<empty1[q];
        filled1p |= 1<<q;
        s1  += offset1[i][q];
        s0p += offset0[i][q];
      }

      // Print board
      MD({
        const auto board = flip_board(pack(side0,root1|set_side(k1,sets1[s1])),(slice+n)&1);
        slog("\ncomputing slice %s+%d, k0 %d, k1 %d, fill %s, board %s, s0 %d, s0p %d, s1p %d, s1 %d",
             slice, n, k0, k1, str_set(k0,set0,k1,sets1[s1],(slice+n)&1),
             board, s0, s0p, s1p, s1);
      })

      // Consider each move in turn
      halfsupers_t us;
      {
        us[0] = us[1] = halfsuper_t(0);
        for (int i=0;i<spots-k0;i++)
          if (!(filled1p&1<<i)) {
            const int cs0 = child_s0s[i];
            const auto cwins = child_wins0[i];
            const halfsupers_t child = input(cs0,cs1ps[s1p][i]);
            us[0] |= cwins | child[0]; // win
            us[1] |= cwins | child[1]; // not lose
            MD({
              const auto child_set0 = set0|side_t(empty1[i])<<5*k0;
              const auto board = pack(root1|set_side(k1,sets1[s1]),root0|set_side(k0+1,child_set0));
              const auto flip = flip_board(board,(slice+n+1)&1);
              slog("child %s, board %s, set %s, s0 %d, s1 %d, cs0 %d, cs1p %s, input %s",
                   str_set(k1,sets1[s1],k0+1,child_set0,(slice+n+1)&1), flip, child_set0,
                   s0, s1, cs0, cs1ps[s1p][i], input.sizes());
              const bool p = (n+parity)&1;
              GEODE_ASSERT(child.x==split(rmax(~super_evaluate_all(false,100,board)))[p]);
              GEODE_ASSERT(child.y==split(rmax(~super_evaluate_all(true ,100,board)))[p]);
            })
          }
      }

      // Account for immediate results
      const halfsuper_t wins1 = all_wins1[s1],
                        inplay = ~(wins0 | wins1);
      us[0] = (inplay & us[0]) | (wins0 & ~wins1); // win
      us[1] = (inplay & us[1]) | (slice+n==36 ? wins0 | ~wins1 : wins0); // not lose

      // Test if desired
      MD({
        const auto board = pack(side0,root1|set_side(k1,sets1[s1]));
        const bool p = (n+parity)&1;
        GEODE_ASSERT(us[0]==split(super_evaluate_all(true ,100,board))[p]);
        GEODE_ASSERT(us[1]==split(super_evaluate_all(false,100,board))[p]);
      })

      // If we're far enough along, remember results
      if (n <= 1) {
        superinfos_t info;
        const auto all = ~halfsuper_t(0);
        info.known   = (n+parity)&1 ? merge(0, all) : merge( all,0);
        info.win     = (n+parity)&1 ? merge(0,us[0]) : merge(us[0],0);
        info.notlose = (n+parity)&1 ? merge(0,us[1]) : merge(us[1],0);
        const uint32_t filled_black = (slice+n)&1 ? filled1 : filled0,
                       filled_white = (slice+n)&1 ? filled0 : filled1;
        board_t board = root;
        for (int i=0;i<spots;i++)
          board += ((filled_black>>i&1)+2*(filled_white>>i&1))*pack(side_t(1)<<empty[i],side_t(0));
        results.insert(make_pair(board,info));
      }

      // Negate and apply rmax in preparation for the slice above
      halfsupers_t above;
      above[0] = rmax(~us[1]);
      above[1] = rmax(~us[0]);
      output(s1,s0p) = above;
      MD(GEODE_ASSERT(!written(s1,s0p)++));
    }
  }
}

unordered_map<board_t,superinfos_t> midsolve_internal(const board_t root, const bool parity,
                                                      RawArray<uint8_t> workspace) {
  high_board_t(root,false); // Verify that we have a consistent board
  const int slice = count_stones(root);
  GEODE_ASSERT(18<=slice && slice<=36);
  const int spots = 36-slice;

  // Make sure workspace will do
  const int align = int(sizeof(halfsupers_t)),
            fix = (align-(uint64_t(workspace.data())&(align-1)))&(align-1);
  GEODE_ASSERT(workspace.size()>=fix);
  const auto safe_workspace = workspace.slice(fix,workspace.size()-fix);
  GEODE_ASSERT(safe_workspace.size()>=bottleneck(spots));

  // Compute all slices
  unordered_map<board_t,superinfos_t> results;
  #define N(n) case n: midsolve_loop<s,n<=s?n:0>(root,parity,results,safe_workspace);
  #define S(s_) \
    case s_: { \
      const int s = (s_); \
      switch (s) { \
        N(18) N(17) N(16) N(15) N(14) N(13) N(12) N(11) N(10) N(9) N(8) N(7) N(6) N(5) N(4) N(3) N(2) N(1) N(0) \
      } \
      break; \
    }
  switch (spots) {
    S(0) S(1) S(2) S(3) S(4) S(5) S(6) S(7) S(8) S(9) S(10) S(11) S(12) S(13) S(14) S(15) S(16) S(17) S(18)
  }
  return results;
}

unordered_map<board_t,int> midsolve(const board_t root, const bool parity,
                                    const RawArray<const board_t> boards, RawArray<uint8_t> workspace) {
  // Compute
  const auto supers = midsolve_internal(root,parity,workspace);

  // Extract results
  unordered_map<board_t,int> results;
  for (const auto board : boards) {
    // We can't superstandardize, since that can break parity.  Instead, we check all local rotations manually.
    for (int i=0;i<256;i++) {
      const local_symmetry_t s(i);
      const auto p = get_pointer(supers, transform_board(s,board));
      if (p) {
        const superinfos_t r = transform_superinfos(s.inverse(),*p);
        if (r.known(0)) {
          results.insert(make_pair(board,r.win(0)+r.notlose(0)-1));
          goto found;
        }
      }
    }
    throw RuntimeError(format("midsolve failure: root %lld, parity %d, board %lld\n%s\n%s",
      root,parity,board,str_board(root),str_board(board)));
    found:;
  }
  return results;
}

unordered_map<high_board_t,int>
high_midsolve(const high_board_t& root, RawArray<const high_board_t> boards,
              RawArray<uint8_t> workspace) {
  vector<board_t> bs;
  for (auto b : boards) {
    GEODE_ASSERT(!b.middle);
    bs.push_back(b.board);
  }
  unordered_map<high_board_t,int> results;
  for (auto [board, value] : midsolve(root.board, root. middle, bs, workspace))
    results.insert(make_pair(high_board_t(board, false), value));
  return results;
}

}
#endif