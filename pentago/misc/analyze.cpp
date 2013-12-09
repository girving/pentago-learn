// Various analysis code

#include <pentago/base/section.h>
#include <pentago/base/superscore.h>
#include <pentago/utility/thread.h>
#include <pentago/utility/wall_time.h>
#include <geode/array/Array2d.h>
#include <geode/geometry/Box.h>
#include <geode/math/uint128.h>
#include <geode/python/wrap.h>
#include <geode/random/Random.h>
#include <geode/utility/range.h>
namespace pentago {

// Generate n random boards, and count the number of (boring positions, immediate black wins, immediate wins white, and ties)
Vector<int,4> sample_immediate_endings(Random& random, section_t section, int samples) {
  Vector<int,4> counts;
  for (int i=0;i<samples;i++) {
    const board_t board = random_board(random,section);
    const side_t side0 = unpack(board,0),
                 side1 = unpack(board,1);
    const super_t wins0 = super_wins(side0),
                  wins1 = super_wins(side1);
    counts[1] += popcount(wins0&~wins1);
    counts[2] += popcount(~wins0&wins1);
    counts[3] += popcount(wins0&wins1);
  }
  counts[0] = 256*samples-counts.sum();
  return counts;
}

// Merge consecutive time intervals separated by at most a threshold
Array<Vector<wall_time_t,2>> simplify_history(RawArray<const history_t> history, int threshold) {
  Array<Vector<wall_time_t,2>> merged;
  for (int i=0;i<history.size();i++) {
    const wall_time_t start = history[i].start;
    wall_time_t end = history[i].end;
    while (history.valid(i+1) && (history[i+1].start-end).us<=threshold)
      end = history[++i].end;
    merged.append(vec(start,end));
  }
  return merged;
}

// Rasterize a piece of history data into an rgba image
void rasterize_history(RawArray<Vector<real,4>,2> image, const Vector<real,2> sizes, RawArray<const history_t> history, const Box<real> y_range, const Vector<real,4> color) {
  typedef Vector<real,2> TV;
  typedef Vector<int,2> IV;
  auto scales = TV(image.sizes()+1)/sizes;
  scales.x *= 1e-6;
  const auto scaled_y_range = scales.y*y_range;
  for (const auto event : history) {
    const Box<TV> box(vec(scales.x*event.start.us,scaled_y_range.min),vec(scales.x*event.end.us,scaled_y_range.max));
    const Box<IV> ibox(clamp_min(IV(floor(box.min)),0),clamp_max(IV(ceil(box.max)),image.sizes()));
    for (int i : range(ibox.min.x,ibox.max.x))
      for (int j : range(ibox.min.y,ibox.max.y))
        image(i,j) += color*Box<TV>::intersect(box,Box<TV>(TV(i,j),TV(i+1,j+1))).volume();
  }
}

// A benchmark for threefish random numbers
uint128_t threefry_benchmark(int n) {
  GEODE_ASSERT(n>=0);
  uint128_t result = n;
  for (int i=0;i<n;i++)
    result = threefry(result,i);
  return result;
}

}
using namespace pentago;

void wrap_analyze() {
  GEODE_FUNCTION(sample_immediate_endings)
  GEODE_FUNCTION(simplify_history)
  GEODE_FUNCTION(rasterize_history)
  GEODE_FUNCTION(threefry_benchmark)
}