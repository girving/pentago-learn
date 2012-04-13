#!/usr/bin/env python
'''Pentago precomputation code'''

from __future__ import division
import os
import sys
from numpy import *
from other.core import *
from other.core.value import parser

def qbit(x,y):
  return 1<<(3*x+y)

def popcount(x):
  count = 0
  while x:
    x &= ~(x&-x)
    count += 1
  return count

@cache
def small_popcounts():
  return asarray([popcount(i) for i in xrange(512)])

def bits(x):
  if not x:
    return '0b0'
  s = ''
  while x:
    s = '01'[x&1]+s
    x //= 2
  return '0b'+s

def ahash(*args):
    return hash(''.join(asarray(a).tostring() for a in args))

@cache
def win_patterns():
  def bit(x,y,flip=False,rotate=False):
    assert 0<=x<6
    assert 0<=y<6
    if flip:
      x,y = y,x
    if rotate:
      x,y = 5-y,x
    return 1<<(16*(2*(x//3)+y//3)+3*(x%3)+y%3)

  # List the various ways of winning
  wins = []
  # Off-centered diagonal 5 in a row
  for x in 0,1:
    for y in 0,1:
      p = 0
      for i in -2,-1,0,1,2:
        p |= bit(2+x+i,2+y+i*(-1 if x==y else 1))
      wins.append(p)
  # Axis aligned 5 in a row
  for flip in 0,1:
    for x in 0,1:
      for y in xrange(6):
        p = 0
        for i in xrange(5):
          p |= bit(x+i,y,flip=flip)
        wins.append(p)
  # Centered diagonal 5 in a row
  for rotate in 0,1:
    for x in 0,1:
      p = 0
      for i in xrange(5):
        p |= bit(i+x,i+x,rotate=rotate)
      wins.append(p)
  wins = asarray(wins)

  # Check that 5 bits are set in each
  for w in wins:
    assert popcount(w)==5

  # There should be 4*3*2+4+4 == 32 ways to win.
  # The first four of these are special: they require contributions from three quadrants.
  # The remaining 28 require contributions from only two quadrants.
  assert len(wins)==32==4*3*2+4+4
  return wins

@cache
def win_contributions():
  wins = win_patterns()
  # For each possible value of each quadrant, determine all the ways that quadrant can
  # contribute to victory.
  table = zeros((4,512),dtype=int64)
  for qx in xrange(2):
    for qy in xrange(2):
      q = 2*qx+qy
      qb = 0x1ff<<16*q
      for v in xrange(512):
        b = v<<16*q
        for i,w in enumerate(wins):
          if w&qb and not ~b&(w&qb):
            table[q,v] |= 1<<(2*i)
  assert ahash(table)==5134745501271341312
  return table

@cache
def show_win_contributions():
  return [('uint64_t','win_contributions','0x%xL',win_contributions())]

@cache
def rotations():
  table = zeros((512,2),dtype=int16)
  for v in xrange(512):
    left = 0
    right = 0
    for x in xrange(3):
      for y in xrange(3):
        if v&qbit(x,y):
          left |= qbit(2-y,x)
          right |= qbit(y,2-x)
    assert popcount(v)==popcount(left)==popcount(right)
    table[v] = left,right
  assert ahash(table)==-631278374917950784
  return table

@cache
def show_rotations():
  return [('uint16_t','rotations','0x%x',rotations())]

@cache
def rotated_win_contributions():
  wins = win_contributions()
  rot = rotations()
  rwins = wins|wins[:,rot[:,0]]|wins[:,rot[:,1]]
  deltas = rwins - wins
  return [('uint64_t','rotated_win_contribution_deltas','0x%xL',deltas)]

def if_(c,a,b):
  c = c!=0
  return c*a+(1-c)*b

@cache
def unrotated_win_distances():
  # Work out the various ways of winning
  patterns = win_patterns().reshape(2,16)

  # Precompute small popcounts
  count = small_popcounts()

  # Generate a distance table giving the total number of moves required for player 0 (black) to
  # reach the pattern, or 4 for unreachable (a white stone is in the way).  Each pattern distance
  # has 3 bits (for 0,1,2,3 or 4=infinity) plus an extra 0 bit to stop carries at runtime, so we
  # fit 16 patterns into each of 2 64-bit ints
  table = zeros((4,3**9,2),int64) # indexed by quadrant,position,word
  unpack_ = unpack()
  for i,pats in enumerate(patterns):
    for j,pat in enumerate(pats):
      for q in xrange(4):
        b = arange(3**9)
        s0,s1 = unpack_[b].T
        qp = (pat>>16*q)&0x1ff
        d = if_(s1&qp,4,count[qp&~s0])
        table[q,b,i] |= d<<4*j
  assert ahash(table)==8251429364477897294
  return [('uint64_t','unrotated_win_distances','0x%xL',table)]

@cache
def arbitrarily_rotated_win_distances():
  # Work out the various ways of winning, allowing arbitrarily many rotations
  patterns = win_patterns().reshape(2,16)

  # Precompute table of multistep rotations
  rotate = empty((512,4),int16)
  rotate[:,0] = arange(512)
  for i in xrange(3):
    rotate[:,i+1] = rotations()[rotate[:,i],0]

  # Precompute small popcounts
  count = small_popcounts()

  # Generate a distance table giving the total number of moves required for player 0 (black) to
  # reach the pattern, or 4 for unreachable (a white stone is in the way).  Each pattern distance
  # has 3 bits (for 0,1,2,3 or 4=infinity) plus an extra 0 bit to stop carries at runtime, so we
  # fit 16 patterns into each of 2 64-bit ints
  table = zeros((4,3**9,2),int64) # indexed by quadrant,position,word
  unpack_ = unpack()
  for i,pats in enumerate(patterns):
    for j,pat in enumerate(pats):
      for q in xrange(4):
        b = arange(3**9)
        s0,s1 = unpack_[b].T
        s0 = rotate[s0,:]
        s1 = rotate[s1,:]
        qp = (pat>>16*q)&0x1ff
        d = (if_(s1&qp,4,count[qp&~s0])).min(axis=-1)
        table[q,b,i] |= d<<4*j
  assert ahash(table)==-3594567715319576186
  return [('uint64_t','arbitrarily_rotated_win_distances','0x%xL',table)]

@cache
def rotated_win_distances():
  # Work out the various ways of winning, allowing at most one quadrant to rotate
  patterns = win_patterns()
  assert len(patterns)==32
  rotated_patterns = [] # List of pattern,rotated_quadrant pairs
  for pat in patterns:
    for q in xrange(4):
      if pat&(0x1ff<<16*q):
        rotated_patterns.append((pat,q))
  assert len(rotated_patterns)==68
  rotated_patterns = asarray(rotated_patterns,object).reshape(4,17,2)

  # Precompute small popcounts
  count = small_popcounts()

  # Generate a distance table giving the total number of moves required for player 0 (black) to
  # reach the pattern, or 4 for unreachable (a white stone is in the way).  Each pattern distance
  # has 3 bits (for 0,1,2,3 or 4=infinity), so we fit 17 patterns into each of 4 64-bit ints
  table = zeros((4,3**9,4),int64) # indexed by quadrant,position,word
  unpack_ = unpack()
  rotations_ = rotations()
  for i,pats in enumerate(rotated_patterns):
    for j,(pat,qr) in enumerate(pats):
      for q in xrange(4):
        b = arange(3**9)
        s0,s1 = unpack_[b].T
        qp = (pat>>16*q)&0x1ff
        d = if_(s1&qp,4,count[qp&~s0])
        if q==qr:
          for r in 0,1:
            d = minimum(d,if_(rotations_[s1,r]&qp,4,count[qp&~rotations_[s0,r]]))
        table[q,b,i] |= d<<3*j
  assert ahash(table)==-1851827180569519993
  return [('uint64_t','rotated_win_distances','0x%xL',table)]

@cache
def reflections():
  table = zeros(512,dtype=int16)
  for v in xrange(512):
    r = 0
    for x in xrange(3):
      for y in xrange(3):
        if v&qbit(x,y):
          r |= qbit(2-y,2-x) # Reflect about x = y line
    assert popcount(v)==popcount(r)
    table[v] = r
  assert ahash(table)==-2942482400364239360
  return [('uint16_t','reflections','0x%x',table)]

'''There are 3**9 = 19683 possible states in each quadrant.  3**9 < 2**16, so we can store
a quadrant state in 16 bits using radix 3.  However, radix 3 is inconvenient for computation,
so we need lookup tables to go to and from packed form.'''
@cache
def pack():
  pack = zeros(2**9,dtype=int16)
  for v in xrange(512):
    pv = 0
    for i in xrange(9):
      if v&2**i:
        pv += 3**i
    pack[v] = pv
  assert ahash(pack)==3488108636539942373
  return pack

@cache
def show_pack():
  return [('uint16_t','pack_table','%d',pack())]

@cache
def unpack():
  unpack = zeros((3**9,2),dtype=int16)
  for v in xrange(3**9):
    vv = v
    p0,p1 = 0,0
    for i in xrange(9):
      c = vv%3
      if c==1:
        p0 += 2**i
      elif c==2:
        p1 += 2**i
      vv //= 3
    unpack[v] = p0,p1
  assert ahash(unpack)==-3185216453998868236
  return unpack

def show_unpack():
  return [('uint16_t','unpack_table','0x%x',unpack())]

# pack and unpack should be inverses
assert all(pack()[unpack()[:,0]]+2*pack()[unpack()[:,1]]==arange(3**9))

@cache
def moves():
  '''Given a quadrant, compute all possible moves by player 0 as a function of the set of filled spaces, stored as a nested array.'''
  moves = []
  for filled in xrange(2**9):
    mv = []
    free = ~filled
    for i in xrange(9):
      b = 1<<i
      if free&b:
        mv.append(b)
    assert len(mv)==9-popcount(filled)
    moves.append(mv)
  # Pack into a flat nested array
  sizes = asarray(map(len,moves))
  offsets = hstack([0,cumsum(sizes)])
  flat = asarray([x for mv in moves for x in mv])
  assert len(flat)==offsets[-1]
  assert len(flat)<2**16
  assert ahash(offsets)==-1634244574290319362
  assert ahash(flat)==72249306322598272
  return [('uint16_t','move_offsets','%d',offsets)
         ,('uint16_t','move_flat','%d',flat)]

@cache
def superwin_info():
  all_rotations = empty((512,4),int16)
  all_rotations[:,0] = arange(512)
  for i in xrange(3):
    all_rotations[:,i+1] = rotations()[all_rotations[:,i],0]
  ways = win_patterns().view(int16).reshape(32,4)
  types = dict(map(reversed,enumerate('h v dl dh da'.split()))) # horizontal, vertical, diagonal lo/hi/assist
  patterns = 'v v - - | h - h - | - h - h | - - v v | dl - da dl | dh da - dh | da dl dl - | - dh dh da'
  patterns = [p.split() for p in patterns.split('|')]
  info = zeros((4,512,5,4,4,4,4),bool)
  for pattern in patterns:
    debug = False # pattern=='- dh dh da'.split()
    if debug: b = [0,136,50,64]
    assert len(pattern)==4
    used   = [i for i in xrange(4) if pattern[i]!='-']
    unused = [i for i in xrange(4) if pattern[i]=='-']
    assert len(used) in (2,3)
    ws = asarray([w for w in ways if all([bool(w[i])==(pattern[i]!='-') or pattern[i]=='da' for i in xrange(4)])])
    assert len(ws) in (6,3)
    assert len(ws)<=4**(4-len(used))
    for q in used:
      for i,w in enumerate(ws[:,q]):
        s = info[q,:,types[pattern[q]]][:,None,None,None,None,...].swapaxes(4,5+q)
        for j,u in enumerate(unused):
          s = s.swapaxes(1+j,5+u)
        x = (all_rotations&w)==w
        if debug:
          print 'pattern %s, q %d, i %d, w %d (%s), x[%d] %s'%(pattern,q,i,w,' '.join(str(i) for i in xrange(9) if w&1<<i),b[q],x[b[q]])
          before = info.copy()
        if len(unused)==1:
          s[:,i] |= x[:,None,None,:,None,None,None,None]
        else:
          s[:,i//4,i%4] |= x[:,None,:,None,None,None,None]
        if 0 and debug:
          changed = zip(*nonzero(info[q,b[q]]!=before[q,b[q]]))
          print '  changed = %d'%len(changed)
          for c in changed:
            print '  %s'%(c,)
  # Pack into 64-bit chunks
  info = packbits(info.astype(int8).swapaxes(-4,-1).swapaxes(-3,-2).reshape(-1,8)[:,::-1]).view(uint64).reshape(4,512,5,4)
  assert ahash(info)==-6962575331390442688
  return [('uint64_t','superwin_info','0x%xL',info)]

@cache
def commute_global_local_symmetries():
  # Represent the various symmetries as subgroups of S_{6*6}
  n = arange(6)
  identity = empty((6,6,2),int8)
  identity[:,:,0] = n.reshape(-1,1)
  identity[:,:,1] = n.reshape(1,-1)
  reflect = identity[n[::-1].reshape(1,-1),n[::-1].reshape(-1,1)]
  assert all(reflect[0,0]==[5,5])
  assert all(reflect[5,0]==[5,0])
  gr = identity[n[::-1].reshape(1,-1),n.reshape(-1,1)]
  assert all(gr[0,0]==[5,0])
  assert all(gr[5,0]==[5,5])
  lr = asarray([identity.copy() for q in xrange(4)])
  n = arange(3)
  for qx in 0,1:
    for qy in 0,1:
      q = 2*qx+qy
      lq = lr[q,3*qx:,3*qy:][:3,:3]
      lq[:] = lq[n[::-1].reshape(1,-1),n.reshape(-1,1)]
      assert all(lr[q,3*qx,3*qy]==[3*qx+2,3*qy])
      assert all(lr[q,3*qx+2,3*qy]==[3*qx+2,3*qy+2])

  # Flattten everything from S_{6*6} to S_36
  def flatten(p):
    return (6*p[...,0]+p[...,1]).reshape(*(p.shape[:-3]+(36,)))
  identity = flatten(identity)
  reflect = flatten(reflect)
  gr = flatten(gr)
  lr = flatten(lr)
  for f in identity,reflect:
    assert all(f[f]==identity)
  for r in (gr,)+tuple(lr):
    assert all(r[r[r[r]]]==identity)

  # Construct local rotation group
  def powers(g):
    p = empty((4,36),int)
    p[0] = identity
    for i in xrange(3):
      p[i+1] = g[p[i]]
    return p
  plr = asarray(map(powers,lr))
  local = empty((4,4,4,4,36),int)
  for i0 in xrange(4):
    for i1 in xrange(4):
      for i2 in xrange(4):
        for i3 in xrange(4):
          local[i0,i1,i2,i3] = plr[0,i0,plr[1,i1,plr[2,i2,plr[3,i3]]]]
  local = local.reshape(256,36)
  # Verify commutativity
  prod = local[:,local]
  assert all(prod==prod.swapaxes(0,1))

  # Construct global rotation group
  globe = empty((2,4,36),int)
  globe[0] = powers(gr)
  for i in xrange(4):
    globe[1,i] = reflect[globe[0,i]]
  globe = globe.reshape(8,36)
  inv_globe = empty_like(globe)
  for i in xrange(8):
    inv_globe[i,globe[i]] = identity

  # Compute conjugations
  def inv(p):
    ip = empty_like(p)
    ip[p] = identity
    return ip
  i = arange(8).reshape(-1,1)
  j = arange(256).reshape(1,-1)
  conjugations = asarray([inv(g)[local[:,g]] for g in globe])

  # Generate lookup table
  table = all(conjugations==local.reshape(-1,1,1,36),axis=-1).argmax(axis=0)
  assert table.shape==(8,256)
  assert ahash(table)==4955614325833109504
  return [('uint8_t','commute_global_local_symmetries','%d',table)]

@cache
def superstandardize_table():
  # Given the relative rankings of the four quadrants, determine the global rotation that minimizes the board value
  rotate = empty((4,4),int) 
  rotate[0] = arange(4)
  rotate[1] = [2,0,3,1]
  rotate[2] = rotate[1,rotate[1]]
  rotate[3] = rotate[2,rotate[1]]
  def v(i):
    shape = ones(5,int)
    shape[i] = 4
    return arange(4).reshape(*shape)
  table = sum([v(i)*4**rotate[v(4),i] for i in xrange(4)]).argmin(axis=-1).T.ravel()
  assert ahash(table)==-1406557380251171136
  return [('uint8_t','superstandardize_table','%d',table)]

def cpp_sizes(data):
  return ''.join('[%d]'%n for n in data.shape)

def cpp_init(format,data):
  if data.shape:
    return '{'+','.join(cpp_init(format,d) for d in data)+'}'
  else:
    return format%data

prefix = PropManager.add('prefix','gen').set_help('directory to store generated files')

def save(filename,tables):
  note = '// Autogenerated by precompute: do not edit directly'
  for header in 0,1:
    file = open('%s/%s.%s'%(prefix(),filename,('h' if header else 'cpp')),'w')
    print>>file, note
    if header:
      print>>file, '#pragma once\n\n#include <stdint.h>'
    else:
      print>>file, '#include "%s.h"'%filename
    print>>file, 'namespace pentago {\n'
    for type,name,format,data in tables:
      if header:
        print>>file, 'extern const %s %s%s;'%(type,name,cpp_sizes(data))
      else:
        print>>file, 'const %s %s%s = %s;'%(type,name,cpp_sizes(data),cpp_init(format,data))
    print>>file, '\n}'

if __name__=='__main__':
  parser.parse('Pentago precomputation script',positional=[prefix])
  if not os.path.exists(prefix()):
    os.makedirs(prefix())
  save('tables',superwin_info()
               +superstandardize_table()
               +commute_global_local_symmetries()
               +show_win_contributions()
               +rotated_win_contributions()
               +unrotated_win_distances()
               +arbitrarily_rotated_win_distances()
               +rotated_win_distances()
               +show_rotations()
               +reflections()
               +show_pack()
               +show_unpack()
               +moves())