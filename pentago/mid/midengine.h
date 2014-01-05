// Solve positions near the end of the game using downstream retrograde analysis
#pragma once

#include <pentago/mid/halfsuper.h>
#include <pentago/high/board.h>
#include <geode/structure/Hashtable.h>
namespace pentago {

// (win,notlose)
typedef Vector<halfsuper_t,2> halfsupers_t;
struct superinfos_t {
  super_t known, win, notlose;
};

// Allocate enough memory for midsolves with at least the given number of stones
GEODE_EXPORT Array<uint8_t> midsolve_workspace(const int min_slice);

// Determine workspace memory required for midsolve.
// Use midsolve_workspace if you're in C++ or Python; this one is for Javascript.
GEODE_EXPORT uint64_t midsolve_workspace_memory_usage(const int min_slice);

// Compute the values of a board and its children, assuming the board has at least 18 stones.
GEODE_EXPORT Hashtable<board_t,superinfos_t> midsolve_internal(const board_t root, const bool parity, RawArray<uint8_t> workspace);

// Compute the values of the given boards, which must be children of a single board
GEODE_EXPORT Hashtable<board_t,int> midsolve(const board_t root, const bool parity,
                                             const RawArray<const board_t> boards, RawArray<uint8_t> workspace);

// High level version of midsolve
GEODE_EXPORT Hashtable<Ref<const high_board_t>,int>
high_midsolve(const high_board_t& root, const vector<Ref<const high_board_t>>& boards, RawArray<uint8_t> workspace);

}