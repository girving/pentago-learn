// Operations involving pentago symmetries
#pragma once

#include "board.h"
#include "superscore.h"
#include <other/core/structure/Tuple.h>
namespace pentago {

using std::ostream;

// The symmetry group of rotation abstracted Pentago is a semidirect product of the
// board rotation group D_4 and the quadrant group Z_4^4.  Despite the fact that
// the rest of the rotation abstracted code is prefixed with super, I somehow
// managed to resist calling this class supersymmetry_t.

// In the following comments, x' is the inverse of x.

struct symmetry_t {
  // The following operations composed as reflect . global . local

  // A 1 bit reflect field and A 2-bit counterclockwise rotation field for the board as a whole
  unsigned global : 3;

   // 4 2-bit counterclockwise rotation fields, one per quadrant
  unsigned local : 8;

  symmetry_t()
    : global(0), local(0) {}

  symmetry_t(uint8_t global, uint8_t local)
    : global(global), local(local) {}

  symmetry_t operator*(symmetry_t s) const {
    // We seek x = g l sg sl = global local s.global s.local
    // Commute local and global transforms: l sg = sg (sg' l sg) = sg l2, x = g sg l2 sl
    uint8_t l2 = commute_global_local_symmetries[s.global][local];
    // Unit tests to the rescue
    return symmetry_t(((global^s.global)&4)|(((global^(global&s.global>>2)<<1)+s.global)&3),
                      (((l2&0x33)+(s.local&0x33))&0x33)+(((l2&0xcc)+(s.local&0xcc))&0xcc));
  }

  symmetry_t inverse() const {
    uint8_t li = local^(local&0x55)<<1; // Invert local
    uint8_t gi = global^(~global>>2&(global&1))<<1; // Invert global
    // Commute local through global
    return symmetry_t(gi,commute_global_local_symmetries[gi][li]);
  }

  bool operator==(symmetry_t s) const {
    return global==s.global && local==s.local;
  }

  bool operator!=(symmetry_t s) const {
    return !(*this==s);
  }

  friend int hash_reduce(symmetry_t s) {
    return s.global<<8|s.local;
  }
};

// A symmetry is a function s : X -> X, where X = Z_6^2 is the set of Pentago squares.
// A side_t is a subset A of X, and a board_t is two such subsets.  These functions compute s(A).
// For example, transform_board(symmetry_t(1,0),board) rotates the whole board left by 90 degrees.
side_t transform_side(symmetry_t s, side_t side) OTHER_CONST;
board_t transform_board(symmetry_t s, board_t board) OTHER_CONST;

// Let B be the set of boards, and A \subset B a subset of boards invariant to global transforms
// (b in A iff g(b) in A for g in D_4).  Define the super operator SA : B -> 2^L by
// SA(b) = {x in L | x(b) in A}.  We want symmetries to act on 2^L s.t. s(SA(b)) = SA(s(b)).
// let s = gy for g in D_4, y in L.  Given C in 2^L s.t. C = SA(b), we have 
//   x in s(C) iff x in s(SA(b)) = SA(s(b))
//             iff x(s(b)) = (xgy)(b) = g(g'xg)y(b) in A iff (g'xgy)(b) in A
//             iff g'xgy in C
// Thus, we define
//   s(C) = gy(C) = {x in L | g'xgy in C} = {gzy'g' | z in C}
super_t transform_super(symmetry_t s, super_t super) OTHER_CONST;

// Given b, find s minimizing s(b), and return s(b),s
Tuple<board_t,symmetry_t> superstandardize(board_t board) OTHER_CONST;

ostream& operator<<(ostream& output, symmetry_t s) OTHER_EXPORT;

}