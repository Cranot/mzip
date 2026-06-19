/* lpaq1.cpp file compressor, July 24, 2007.
(C) 2007, Matt Mahoney, matmahoney@yahoo.com

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

To compress:   N input output  (N=0..9, uses 3 + 3*2^N MB memory)
To decompress: d input output  (requires same memory)

For example:

  lpaq1 9 foo foo.lpq

compresses foo to foo.lpq using 1.5 GB memory.

  lpaq1 d foo.lpq foo

decompresses to foo, also using 1.5 GB memory.  Option 0 uses
6 MB memory.  The maximum file size is 2 GB.

lpaq1 is a "lite" version of PAQ, about 35 times faster than paq8l
at the cost of some compression (but similar to high end BWT and
PPM compressors).  It is a context-mixing compressor combining 7 
contexts: orders 1, 2, 3, 4, 6, a lowercase unigram word context 
(for ASCII text), and a "match" order, which predicts the next
bit in the last matching context.  The independent bit predictions of
the 7 models are combined using one of 80 neural networks (selected by
a small context), then adjusted using 2 SSE stages (order 0 and 1)
and arithmetic coded.

Prediction is bitwise.  This means that an order-n context consists
of the last n whole bytes plus any of the 0 to 7 previously coded
bits of the current byte starting with the most significant bit.
The unigram word context consists of a hash of the last (at most) 11
consecutive letters (A-Z, a-z) folded to lower case.  The context
does not include any nonalphabetic characters nor any characters
preceding the last nonalphabetic character.

The first 6 contexts (orders 1..4, 6, word) are used to index a
hash table to look up a bit-history represented by an 8-bit state.
The representable states are the same as in paq8l.  A state can
either represent all histories up to 4 bits long, or a pair of
0,1 counts plus a flag to indicate the most recent bit.  The counts
are bounded by (41,0), (40,1), (12,2), (5,3), (4,4) and likewise
for 1,0.  When a count is exceeded, the opposite count is reduced to 
approximately preserve the count ratio.  The last bit flag is present
only for states whose total count is less than 16.  There are 253
possible states.

A bit history is mapped to a probability using an adaptive table
(StateMap).  This differs from paq8l in that each table entry includes
a count so that adaptation is rapid at first.  Each table entry
has a 22-bit probability (initially p = 0.5) and 10-bit count (initially 
n = 0) packed into 32 bits.  After bit y is predicted, n is incremented
up to the limit (1023) and the probability is adjusted by 
p := p + (y - p)/(n + 0.5).  This model is stationary: 
p = (n1 + 0.5)/(n + 1), where n1 is the number of times y = 1 out of n.

The "match" model (MatchModel) looks up the current context in a
hash table, first using a longer context, then a shorter one.  If
a match is found, then the following bits are predicted until there is
a misprediction.  The prediction is computed by mapping the predicted
bit, the length of the match (1..15 or quantized by 4 in 16..62, max 62),
and the last whole byte as context into a StateMap.  If no match is found,
then the order 0 context (last 0..7 bits of the current byte) is used
as context to the StateMap.

The 7 predictions are combined using a neural network (Mixer) as in
paq8l, except it is a single level network without MMX code.  The
inputs p_i, i=0..6 are first stretched: t_i = log(p_i/(1 - p_i)), 
then the output is computed: p = squash(SUM_i t_i * w_i), where
squash(x) = 1/(1 + exp(-x)) is the inverse of stretch().  The weights
are adjusted to reduce the error: w_i := w_i + L * t_i * (y - p) where
(y - p) is the prediction error and L ~ 0.002 is the learning rate.
This is a standard single layer backpropagation network modified to
minimize coding cost rather than RMS prediction error (thus dropping
the factors p * (1 - p) from learning).

One of 80 neural networks are selected by a context that depends on
the 3 high order bits of the last whole byte plus the context order
(quantized to 0, 1, 2, 3, 4, 6, 8, 12, 16, 32).  The order is
determined by the number of nonzero bit histories and the length of
the match from MatchModel.

The Mixer output is adjusted by 2 SSE stages (called APM for adaptive
probability map).  An APM is a StateMap that accepts both a discrte
context and an input probability, pr.  pr is stetched and quantized
to 24 levels.  The output is interpolated between the 2 nearest
table entries, and then only the nearest entry is updated.  The entries
are initialized to p = pr and n = 6 (to slow down initial adaptation)
with a limit n <= 255.  The APM differs from paq8l in that it uses
the new StateMap rapid initial adaptation, does not update both
adjacent table entries, and uses 24 levels instead of 33.  The two
stages use a discrete order 0 context (last 0..7 bits) and a hashed order-1
context (14 bits).  Each output is averaged with its input weighted
by 1/4.

The output is arithmetic coded.  The code for a string s with probability
p(s) is a number between Q and Q+p(x) where Q is the total probability
of all strings lexicographically preceding s.  The number is coded as
a big-endian base-256 fraction.  A header is prepended as follows:

- "pQ" 2 bytes must be present or decompression gives an error.
- 1 (0x01) version number (other values give an error).
- memory option N as one byte '0'..'9' (0x30..0x39).
- file size as a 4 byte big-endian number.
- arithmetic coded data.

Two thirds of the memory (2 * 2^N MB) is used for a hash table mapping
the 6 regular contexts (orders 1-4, 6, word) to bit histories.  A lookup
occurs every 4 bits.  The input is a byte-oriented context plus possibly
the first nibble of the next byte.  The output is an array of 15 bit
histories (1 byte each) for all possible contexts formed by appending
0..3 more bits.  The table entries have the format:

 {checksum, "", 0, 1, 00, 10, 01, 11, 000, 100, 010, 110, 001, 101, 011, 111}

The second byte is the bit history for the context ending on a nibble
boundary.  It also serves as a priority for replacement.  The states
are ordered by increasing total count, where state 0 represents the
initial state (no history).  When a context is looked up, the 8 bit
checksum (part of the hash) is compared with 3 adjacent entries, and
if there is no match, the entry with the lowest priority is cleared
and the new checksum is stored.

The hash table is aligned on 64 byte cache lines.  A table lookup never
crosses a 64 byte address boundary.  Given a 32-bit hash h of the context,
8 bits are used for the checksum and 17 + N bits are used for the
index i.  Then the entries i, i XOR 1, and i XOR 2 are tried.  The hash h
is actually a collision-free permuation, consisting of multiplying the
input by a large odd number mod 2^32, a 16-bit rotate, and another multiply.

The order-1 context is mapped to a bit history using a 64K direct
lookup table, not a hash table.

One third of memory is used by MatchModel, divided equally between 
a rotating input buffer of 2^(N+19) bytes and an index (hash table)
with 2^(N+17) entries.  Two context hashes are maintained, a long one,
h1, of length ceil((N+17)/3) bytes and a shorter one, h2, of length 
ceil((N+17)/5) bytes, where ceil() is the ceiling function.  The index
does not use collision detection.  At each byte boundary, if there is 
not currently a match, then the bytes before the current byte are
compared with the location indexed by h1.  If less than 2 bytes match,
then h2 is tried.  If a match of length 1 or more is found, the
match is maintained until the next bit mismatches the predicted bit.
The table is updated at h1 and h2 after every byte.

To compile (g++ 3.4.5, upx 3.00w):
  g++ -Wall lpaq1.cpp -O2 -Os -march=pentiumpro -fomit-frame-pointer 
      -s -o lpaq1.exe
  upx -qqq lpaq1.exe

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#define NDEBUG  // remove for debugging
#include <assert.h>
#include <signal.h>

// 8, 16, 32 bit unsigned types (adjust as appropriate)
typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned int   U32;

// Error handler: print message if any, and exit
void quit(const char* message=0) {
  if (message) printf("%s\n", message);
  exit(1);
}

// Create an array p of n elements of type T
template <class T> void alloc(T*&p, size_t n) {
  p=(T*)calloc(n, sizeof(T));
  if (!p) quit("out of memory");
}

int fuzzmode=0;
int autonomousmode=0;
int guessedmode=0;

///////////////////////////// Squash //////////////////////////////

// return p = 1/(1 + exp(-d)), d scaled by 8 bits, p scaled by 12 bits
int squash(int d) {
  static const int t[33]={
    1,2,3,6,10,16,27,45,73,120,194,310,488,747,1101,
    1546,2047,2549,2994,3348,3607,3785,3901,3975,4022,
    4050,4068,4079,4085,4089,4092,4093,4094};
  if (d>2047) return 4095;
  if (d<-2047) return 0;
  int w=d&127;
  d=(d>>7)+16;
  return (t[d]*(128-w)+t[(d+1)]*w+64) >> 7;
}

//////////////////////////// Stretch ///////////////////////////////

// Inverse of squash. stretch(d) returns ln(p/(1-p)), d scaled by 8 bits,
// p by 12 bits.  d has range -2047 to 2047 representing -8 to 8.  
// p has range 0 to 4095 representing 0 to 1.

class Stretch {
  short t[4096];
public:
  Stretch();
  int operator()(int p) const {
    assert(p>=0 && p<4096);
    return t[p];
  }
} stretch;

Stretch::Stretch() {
  int pi=0;
  for (int x=-2047; x<=2047; ++x) {  // invert squash()
    int i=squash(x);
    for (int j=pi; j<=i; ++j)
      t[j]=x;
    pi=i+1;
  }
  t[4095]=2047;
}

///////////////////////// state table ////////////////////////

// State table:
//   nex(state, 0) = next state if bit y is 0, 0 <= state < 256
//   nex(state, 1) = next state if bit y is 1
//
// States represent a bit history within some context.
// State 0 is the starting state (no bits seen).
// States 1-30 represent all possible sequences of 1-4 bits.
// States 31-252 represent a pair of counts, (n0,n1), the number
//   of 0 and 1 bits respectively.  If n0+n1 < 16 then there are
//   two states for each pair, depending on if a 0 or 1 was the last
//   bit seen.
// If n0 and n1 are too large, then there is no state to represent this
// pair, so another state with about the same ratio of n0/n1 is substituted.
// Also, when a bit is observed and the count of the opposite bit is large,
// then part of this count is discarded to favor newer data over old.

static const U8 State_table[256][2]={
{  1,  2},{  3,  5},{  4,  6},{  7, 10},{  8, 12},{  9, 13},{ 11, 14}, // 0
{ 15, 19},{ 16, 23},{ 17, 24},{ 18, 25},{ 20, 27},{ 21, 28},{ 22, 29}, // 7
{ 26, 30},{ 31, 33},{ 32, 35},{ 32, 35},{ 32, 35},{ 32, 35},{ 34, 37}, // 14
{ 34, 37},{ 34, 37},{ 34, 37},{ 34, 37},{ 34, 37},{ 36, 39},{ 36, 39}, // 21
{ 36, 39},{ 36, 39},{ 38, 40},{ 41, 43},{ 42, 45},{ 42, 45},{ 44, 47}, // 28
{ 44, 47},{ 46, 49},{ 46, 49},{ 48, 51},{ 48, 51},{ 50, 52},{ 53, 43}, // 35
{ 54, 57},{ 54, 57},{ 56, 59},{ 56, 59},{ 58, 61},{ 58, 61},{ 60, 63}, // 42
{ 60, 63},{ 62, 65},{ 62, 65},{ 50, 66},{ 67, 55},{ 68, 57},{ 68, 57}, // 49
{ 70, 73},{ 70, 73},{ 72, 75},{ 72, 75},{ 74, 77},{ 74, 77},{ 76, 79}, // 56
{ 76, 79},{ 62, 81},{ 62, 81},{ 64, 82},{ 83, 69},{ 84, 71},{ 84, 71}, // 63
{ 86, 73},{ 86, 73},{ 44, 59},{ 44, 59},{ 58, 61},{ 58, 61},{ 60, 49}, // 70
{ 60, 49},{ 76, 89},{ 76, 89},{ 78, 91},{ 78, 91},{ 80, 92},{ 93, 69}, // 77
{ 94, 87},{ 94, 87},{ 96, 45},{ 96, 45},{ 48, 99},{ 48, 99},{ 88,101}, // 84
{ 88,101},{ 80,102},{103, 69},{104, 87},{104, 87},{106, 57},{106, 57}, // 91
{ 62,109},{ 62,109},{ 88,111},{ 88,111},{ 80,112},{113, 85},{114, 87}, // 98
{114, 87},{116, 57},{116, 57},{ 62,119},{ 62,119},{ 88,121},{ 88,121}, // 105
{ 90,122},{123, 85},{124, 97},{124, 97},{126, 57},{126, 57},{ 62,129}, // 112
{ 62,129},{ 98,131},{ 98,131},{ 90,132},{133, 85},{134, 97},{134, 97}, // 119
{136, 57},{136, 57},{ 62,139},{ 62,139},{ 98,141},{ 98,141},{ 90,142}, // 126
{143, 95},{144, 97},{144, 97},{ 68, 57},{ 68, 57},{ 62, 81},{ 62, 81}, // 133
{ 98,147},{ 98,147},{100,148},{149, 95},{150,107},{150,107},{108,151}, // 140
{108,151},{100,152},{153, 95},{154,107},{108,155},{100,156},{157, 95}, // 147
{158,107},{108,159},{100,160},{161,105},{162,107},{108,163},{110,164}, // 154
{165,105},{166,117},{118,167},{110,168},{169,105},{170,117},{118,171}, // 161
{110,172},{173,105},{174,117},{118,175},{110,176},{177,105},{178,117}, // 168
{118,179},{110,180},{181,115},{182,117},{118,183},{120,184},{185,115}, // 175
{186,127},{128,187},{120,188},{189,115},{190,127},{128,191},{120,192}, // 182
{193,115},{194,127},{128,195},{120,196},{197,115},{198,127},{128,199}, // 189
{120,200},{201,115},{202,127},{128,203},{120,204},{205,115},{206,127}, // 196
{128,207},{120,208},{209,125},{210,127},{128,211},{130,212},{213,125}, // 203
{214,137},{138,215},{130,216},{217,125},{218,137},{138,219},{130,220}, // 210
{221,125},{222,137},{138,223},{130,224},{225,125},{226,137},{138,227}, // 217
{130,228},{229,125},{230,137},{138,231},{130,232},{233,125},{234,137}, // 224
{138,235},{130,236},{237,125},{238,137},{138,239},{130,240},{241,125}, // 231
{242,137},{138,243},{130,244},{245,135},{246,137},{138,247},{140,248}, // 238
{249,135},{250, 69},{ 80,251},{140,252},{249,135},{250, 69},{ 80,251}, // 245
{140,252},{  0,  0},{  0,  0},{  0,  0}};  // 252
#define nex(state,sel) State_table[state][sel]

//////////////////////////// StateMap, APM //////////////////////////

// A StateMap maps a context to a probability.  Methods:
//
// Statemap sm(n) creates a StateMap with n contexts using 4*n bytes memory.
// sm.p(y, cx, limit) converts state cx (0..n-1) to a probability (0..4095).
//     that the next y=1, updating the previous prediction with y (0..1).
//     limit (1..1023, default 1023) is the maximum count for computing a
//     prediction.  Larger values are better for stationary sources.

class StateMap {
protected:
  const int N;  // Number of contexts
  int cxt;      // Context of last prediction
  U32 *t;       // cxt -> prediction in high 22 bits, count in low 10 bits
  static int dt[1024];  // i -> 16K/(i+3)
  void update(int y, int limit) {
    assert(cxt>=0 && cxt<N);
    int n=t[cxt]&1023, p=t[cxt]>>10;  // count, prediction
    if (n<limit) ++t[cxt];
    else t[cxt]=t[cxt]&0xfffffc00|limit;
    t[cxt]+=(((y<<22)-p)>>3)*dt[n]&0xfffffc00;
  }
public:
  StateMap(int n=256);

  // update bit y (0..1), predict next bit in context cx
  int p(int y, int cx, int limit=1023) {
    assert(y>>1==0);
    assert(cx>=0 && cx<N);
    assert(limit>0 && limit<1024);
    update(y, limit);
    /*if(fuzzmode && rand()<RAND_MAX/10000000) {
        int old = cxt;
        cxt=cx;
        return t[old]>>20;
    }*/
    return t[cxt=cx]>>20;
  }
};

int StateMap::dt[1024]={0};

StateMap::StateMap(int n): N(n), cxt(0) {
  alloc(t, N);
  for (int i=0; i<N; ++i)
    t[i]=1<<31;
  if (dt[0]==0)
    for (int i=0; i<1024; ++i)
      dt[i]=16384/(i+i+3);
}

// An APM maps a probability and a context to a new probability.  Methods:
//
// APM a(n) creates with n contexts using 96*n bytes memory.
// a.pp(y, pr, cx, limit) updates and returns a new probability (0..4095)
//     like with StateMap.  pr (0..4095) is considered part of the context.
//     The output is computed by interpolating pr into 24 ranges nonlinearly
//     with smaller ranges near the ends.  The initial output is pr.
//     y=(0..1) is the last bit.  cx=(0..n-1) is the other context.
//     limit=(0..1023) defaults to 255.

class APM: public StateMap {
public:
  APM(int n);
  int pp(int y, int pr, int cx, int limit=255) {
    assert(y>>1==0);
    assert(pr>=0 && pr<4096);
    assert(cx>=0 && cx<N/24);
    assert(limit>0 && limit<1024);
    update(y, limit);
    pr=(stretch(pr)+2048)*23;
    int wt=pr&0xfff;  // interpolation weight of next element
    cx=cx*24+(pr>>12);
    assert(cx>=0 && cx<N-1);
    pr=(t[cx]>>13)*(0x1000-wt)+(t[cx+1]>>13)*wt>>19;
    cxt=cx+(wt>>11);
    return pr;
  }
};

APM::APM(int n): StateMap(n*24) {
  for (int i=0; i<N; ++i) {
    int p=((i%24*2+1)*4096)/48-2048;
    t[i]=(U32(squash(p))<<20)+6;
  }
}

//////////////////////////// Mixer /////////////////////////////

// Mixer m(N, M) combines models using M neural networks with
//     N inputs each using 4*M*N bytes of memory.  It is used as follows:
// m.update(y) trains the network where the expected output is the
//     last bit, y.
// m.add(stretch(p)) inputs prediction from one of N models.  The
//     prediction should be positive to predict a 1 bit, negative for 0,
//     nominally -2K to 2K.
// m.set(cxt) selects cxt (0..M-1) as one of M neural networks to use.
// m.p() returns the output prediction that the next bit is 1 as a
//     12 bit number (0 to 4095).  The normal sequence per prediction is:
//
// - m.add(x) called N times with input x=(-2047..2047)
// - m.set(cxt) called once with cxt=(0..M-1)
// - m.p() called once to predict the next bit, returns 0..4095
// - m.update(y) called once for actual bit y=(0..1).

inline void train(int *t, int *w, int n, int err) {
  for (int i=0; i<n; ++i) {
    w[i]+=t[i]*err+0x8000>>16;
  }
}

inline int dot_product(int *t, int *w, int n) {
  int sum=0;
  for (int i=0; i<n; ++i)
    sum+=t[i]*w[i];
  return sum>>8;
}

class Mixer {
  const int N, M;  // max inputs, max contexts
  int* tx;         // N inputs
  int* wx;         // N*M weights
  int cxt;         // context
  int nx;          // Number of inputs in tx, 0 to N
  int pr;          // last result (scaled 12 bits)
  int rate;        // learning rate (default 7)
public:
  Mixer(int n, int m, int r=7, int winit=0);

  // Adjust weights to minimize coding cost of last prediction
  void update(int y) {
    int err=((y<<12)-pr)*rate;
    train(&tx[0], &wx[cxt*N], N, err);
    nx=0;
  }

  // Input x (call up to N times)
  void add(int x) {
    assert(nx<N);
    tx[nx++]=x;
  }

  // Set a context
  void set(int cx) {
    assert(cx>=0 && cx<M);
    cxt=cx;
  }

  // predict next bit
  int p() {
    return pr=squash(dot_product(&tx[0], &wx[cxt*N], N)>>8);
  }
};

Mixer::Mixer(int n, int m, int r, int winit):
    N(n), M(m), tx(0), wx(0), cxt(0), nx(0), pr(2048), rate(r) {
  alloc(tx, N);
  alloc(wx, N*M);
  if(winit) for(int i=0;i<N*M;i++) wx[i]=winit;
}

//////////////////////////// HashTable /////////////////////////

// A HashTable maps a 32-bit index to an array of B bytes.
// The first byte is a checksum using the upper 8 bits of the
// index.  The second byte is a priority (0 = empty) for hash
// replacement.  The index need not be a hash.

// HashTable<B> h(n) - create using n bytes  n and B must be 
//     powers of 2 with n >= B*4, and B >= 2.
// h[i] returns array [1..B-1] of bytes indexed by i, creating and
//     replacing another element if needed.  Element 0 is the
//     checksum and should not be modified.

template <int B>
class HashTable {
  U8* t;  // table: 1 element = B bytes: checksum priority data data
  const size_t N;  // size in bytes
public:
  HashTable(size_t n);
  U8* operator[](U32 i);
};

template <int B>
HashTable<B>::HashTable(size_t n): t(0), N(n) {
  assert(B>=2 && (B&B-1)==0);
  assert(N>=B*4 && (N&N-1)==0);
  alloc(t, N+B*4+64);
  t+=64-int(((long)t)&63);  // align on cache line boundary
}

template <int B>
inline U8* HashTable<B>::operator[](U32 i) {
  i*=123456791;
  i=i<<16|i>>16;
  i*=234567891;
  int chk=i>>24;
  i=i*B&N-B;
  if (t[i]==chk) return t+i;
  if (t[i^B]==chk) return t+(i^B);
  if (t[i^B*2]==chk) return t+(i^B*2);
#ifdef HT4
  if (t[i^B*3]==chk) return t+(i^B*3);                 // 4-way: use the 4th cache-line slot too
  { U32 a=i, b=i^B, c=i^B*2, d=i^B*3, m=a;             // evict the lowest-priority of the 4
    if(t[b+1]<t[m+1]) m=b; if(t[c+1]<t[m+1]) m=c; if(t[d+1]<t[m+1]) m=d; i=m; }
#else
  if (t[i+1]>t[i+1^B] || t[i+1]>t[i+1^B*2]) i^=B;
  if (t[i+1]>t[i+1^B^B*2]) i^=B^B*2;
#endif
  memset(t+i, 0, B);
  t[i]=chk;
  return t+i;
}

//////////////////////////// MatchModel ////////////////////////

// MatchModel(n) predicts next bit using most recent context match.
//     using n bytes of memory.  n must be a power of 2 at least 8.
// MatchModel::p(y, m) updates the model with bit y (0..1) and writes
//     a prediction of the next bit to Mixer m.  It returns the length of
//     context matched (0..62).

class MatchModel {
  const U32 N;  // last buffer index, bufsize-1
  const U32 HN; // last hash table index, htsize-1
  enum {MAXLEN=62};   // maximum match length, at most 62
  U8* buf;    // input buffer
  int* ht;    // context hash -> next byte in buf
  int pos;    // number of bytes in buf
  int match;  // pointer to current byte in matched context in buf
  int len;    // length of match
  U32 h1, h2; // context hashes
  int c0;     // last 0-7 bits of y
  int bcount; // number of bits in c0 (0..7)
  StateMap sm;  // len, bit, last byte -> prediction
  int indexBak, lenBak, delta, expectedByte;  // MATCH3: delta-mode recovery state
  StateMap sm2;                               // MATCH3: 2nd match StateMap
public:
  MatchModel(U32 bufsize, U32 htsize);
  int p(int y, int* outpr);  // update bit y; write stretched match prediction to *outpr; return len
  int p3(int y, int* out, int* nout);  // MATCH3 full cascade: writes nout inputs to out[]; returns len
};

MatchModel::MatchModel(U32 bufsize, U32 htsize): N(bufsize-1), HN(htsize-1), buf(0), ht(0), pos(0),
    match(0), len(0), h1(0), h2(0), c0(1), bcount(0), sm(56<<8),
    indexBak(0), lenBak(0), delta(0), expectedByte(0), sm2(1<<11) {
  alloc(buf, N+1);
  alloc(ht, HN+1);
}

// MATCH3: full delta-mode-recovery cascade (faithful paq8px MatchModel port, adapted to single-match lpaq).
// Bit-mismatch -> save backup + delta (len=0, predict order-0); recover at next boundary ONLY if verified
// (buf[indexBak]==completed byte); 2nd mismatch in recovery -> give up; stabilize after MINLEN_RM=3 bytes.
int MatchModel::p3(int y, int* out, int* nout) {
  // 1. mismatch check for the bit JUST coded (y) vs what we predicted
  if (len != 0) {
    int eb = (expectedByte >> (7 - bcount)) & 1;
    if (y != eb) {
      if (lenBak != 0) { lenBak = 0; indexBak = 0; }          // recovery 2nd mismatch -> give up
      else { lenBak = len; indexBak = match; delta = 1; }     // enter delta mode
      len = 0;
    }
  }
  // 2. incorporate y; at byte boundary run the recovery/extend/search state machine
  c0 += c0 + y; ++bcount;
  if (bcount == 8) {
    int b = c0 & 0xff;
    bcount = 0;
    h1 = h1*(3<<3)+c0 & HN;
    h2 = h2*(5<<5)+c0 & HN;
    buf[pos++] = c0; c0 = 1; pos &= N;
    if (len==0 && !delta && lenBak!=0) {                       // PRE-RECOVERY (boundary after a delta byte)
      indexBak=(indexBak+1)&N; if (lenBak<MAXLEN) lenBak++;
      if (buf[indexBak]==b) { len=lenBak; match=indexBak; }    // verified recover
      else { lenBak=0; indexBak=0; }                           // give up
    }
    if (len!=0) {                                              // NORMAL EXTENSION
      match=(match+1)&N; if (len<MAXLEN) len++;
      if (lenBak!=0 && (len-lenBak)>=3) { lenBak=0; indexBak=0; }  // stabilize (MINLEN_RM)
    }
    delta = 0;
    if (len==0 && lenBak==0) {                                 // NEW MATCH SEARCH (don't clobber a pending recovery)
      match=ht[h1];
      if (match!=pos) { int i; while (len<MAXLEN && (i=(match-len-1)&N)!=pos && buf[i]==buf[(pos-len-1)&N]) ++len; }
      if (len<2) { len=0; match=ht[h2];
        if (match!=pos) { int i; while (len<MAXLEN && (i=(match-len-1)&N)!=pos && buf[i]==buf[(pos-len-1)&N]) ++len; }
      }
    }
    expectedByte = (len!=0) ? buf[match] : 0;
    ht[h1]=pos; ht[h2]=pos;
  }
  // 3. predict next bit -> 4 mixer inputs
  int n=0;
  int eb = (len!=0) ? ((expectedByte >> (7-bcount)) & 1) : 0;
  int sign = (len!=0) ? (2*eb-1) : 0;
  out[n++] = sign * ((len<32?len:32) << 5);                    // lin confidence
  { int il=0, x=(len<MAXLEN?len:MAXLEN); while(x>1){x>>=1; il+=16;} out[n++] = sign * (il<<2); }  // log confidence
  { int cx = (len!=0) ? ((len<16? len*2+eb : (len>>2)*2+eb+24)*256 + buf[(pos-1)&N]) : 0;
    out[n++] = stretch(sm.p(y, cx)); }                         // StateMap: (len,bit,lastbyte)
  { int cx2 = (len!=0) ? (((expectedByte<<3)|bcount) & 0x7ff) : 0;
    out[n++] = stretch(sm2.p(y, cx2)); }                       // StateMap: (expectedByte,bpos)
  *nout = n;
  return len;
}

int MatchModel::p(int y, int* outpr) {

  // update context
  c0+=c0+y;
  ++bcount;
  if (bcount==8) {
    bcount=0;
    h1=h1*(3<<3)+c0&HN;
    h2=h2*(5<<5)+c0&HN;
    buf[pos++]=c0;
    c0=1;
    pos&=N;

    // find or extend match
#ifdef MATCH2
    { int b=buf[(pos-1)&N];                          // byte just completed (c0 already reset to 1 above)
      if (len>0) {
        if (match!=pos && buf[match]==b) { match=(match+1)&N; if(len<MAXLEN) ++len; }  // continue match
        else { match=(match+1)&N; len=(len>=4)?(len>>1):0; }             // delta-mode: skip 1-byte glitch
      }
      if (len==0) {
        match=ht[h1];
        if (match!=pos) { int i; while(len<MAXLEN && (i=match-len-1&N)!=pos && buf[i]==buf[pos-len-1&N]) ++len; }
        if (len<2) { len=0; match=ht[h2];
          if (match!=pos) { int i; while(len<MAXLEN && (i=match-len-1&N)!=pos && buf[i]==buf[pos-len-1&N]) ++len; }
        }
      }
    }
#else
    if (len>0) {
      ++match;
      match&=N;
      if (len<MAXLEN) ++len;
    }
    else {
      match=ht[h1];
      if (match!=pos) {
        int i;
        while (len<MAXLEN && (i=match-len-1&N)!=pos
               && buf[i]==buf[pos-len-1&N])
          ++len;
      }
    }
    if (len<2) {
      len=0;
      match=ht[h2];
      if (match!=pos) {
        int i;
        while (len<MAXLEN && (i=match-len-1&N)!=pos
               && buf[i]==buf[pos-len-1&N])
          ++len;
      }
    }
#endif
  }

  // predict
  int cxt=c0;
  if (len>0 && (buf[match]+256>>8-bcount)==c0) {
    int b=buf[match]>>7-bcount&1;  // next bit
    if (len<16) cxt=len*2+b;
    else cxt=(len>>2)*2+b+24;
    cxt=cxt*256+buf[pos-1&N];
  }
#ifndef MATCH2
  else
    len=0;
#endif
  *outpr=stretch(sm.p(y, cxt));

  // update index
  if (bcount==0) {
    ht[h1]=pos;
    ht[h2]=pos;
  }
  return len;
}

//////////////////////////// Predictor /////////////////////////

// A Predictor estimates the probability that the next bit of
// uncompressed data is 1.  Methods:
// Predictor(n) creates with 3*n bytes of memory.
// p() returns P(1) as a 12 bit number (0-4095).
// update(y) trains the predictor with the actual bit (0 or 1).

int MEM=0;  // Global memory usage = 3*MEM bytes (1<<20 .. 1<<29)
#ifndef XPN
  #define XPN 0
#endif
#ifdef WIKI
  #ifndef WIKIN
    #define WIKIN 7
  #endif
  #define NWIKI WIKIN
#else
  #define NWIKI 0
#endif
#ifdef IDNUM
  #define NIDNUM 4
#else
  #define NIDNUM 0
#endif
#ifdef DICT
  #define NDICT 2
#else
  #define NDICT 0
#endif
#ifdef MORE
  #define NMORE 5
#else
  #define NMORE 0
#endif
#define NM (19+XPN+NWIKI+NIDNUM+NDICT+NMORE)
#ifdef RUNMAP
  #define NRUN 1
#else
  #define NRUN 0
#endif
#ifdef ISSECH
  #define NCH 1
#else
  #define NCH 0
#endif
#ifdef MATCH3
  #define NMATCH 4
#else
  #define NMATCH 1
#endif
#ifdef ISSE2
  #define NIN (2*NM+NMATCH+NRUN+NCH)
#else
  #define NIN (NM+NMATCH+NRUN+NCH)
#endif
#ifdef FULL
  #ifndef HTBITS
    #define HTBITS 32
  #endif
  #ifndef MMBUF
    #define MMBUF 30
  #endif
  #ifndef MMIDX
    #define MMIDX 28
  #endif
#else
  #define HTBITS 28
  #define MMBUF 25
  #define MMIDX 23
#endif

class Predictor {
  int pr;  // next prediction
public:
  Predictor();
  int p() const {assert(pr>=0 && pr<4096); return pr;}
  void update(int y);
};

Predictor::Predictor(): pr(2048) {}

void Predictor::update(int y) {
  static U8 t0[0x10000];  // order 1 cxt -> state
  static HashTable<16> t(1ull<<HTBITS);  // cxt -> state (FULL=4GB, else 256MB test)
  static int c0=1;  // last 0-7 bits with leading 1
  static int c4=0;  // last 4 bytes
  static unsigned long long c8=0;  // last 8 bytes
  static U32 wordh=0,cword=0,pword=0,grp=0,caseh=0,line0=0,line1=0,container=0,tagacc=0,prevcont=0,suf=0,hlong=0;  // word+case+line+wiki(+MORE)
  static int bcount=0,wpos=0,col=0,ic=0,xmlm=0,intag=0,tagclose=0,wmode2=0,listc=0,fieldpos=0,digrun=0;
  static U8 *cp[NM]; { static int _ini=0; if(!_ini){ for(int i=0;i<NM;i++) cp[i]=t0; _ini=1; } }
  static StateMap sm[NM];
  static APM a1(0x100), a2(0x4000), a3(0x1000);
#ifdef SSE45
  static APM a4(0x100), a5(0x1000);   // 4th: match-state, 5th: structural (container/wmode2)
#endif
  static U32 h[NM];
#ifdef MIXER_2STAGE
  static Mixer m1a(NIN,512,7), m1b(NIN,512,7), m1c(NIN,512,7), m1d(NIN,512,7);
  static Mixer* m1[4]={&m1a,&m1b,&m1c,&m1d};
#ifdef S2RICH
  static Mixer m2(4, 128, 3, 1<<14);  // stage-2 gated by (order, class) — richer specialization
#else
  static Mixer m2(4, 16, 3, 1<<14);   // stage-2: 4 inputs, gated by order, low LR, avg-init weights
#endif
#else
  static Mixer m(NIN, 512);
#endif
  static MatchModel mm(1u<<MMBUF, 1u<<MMIDX);  // FULL=1GB buffer, else 32MB test
#ifdef RUNMAP
  static U16* rt; static StateMap sm_run(256); static U32 runhash=0; static int runpred=0, runcnt=0;
  { static int _rini=0; if(!_rini){ alloc(rt, 1u<<24); _rini=1; } }   // order-3 run records {byte,count}
#endif
#ifdef ISSECH
  static StateMap smc[8];                                    // per-stage secondary StateMaps
  static Mixer* mi[8]; { static int _mii=0; if(!_mii){ for(int k=0;k<8;k++) mi[k]=new Mixer(2,256,4); _mii=1; } }
#endif
#ifdef DICT
  static U32* wct=0; static int wfreq=0, pfreq=0;            // ONLINE word-frequency dict (zero stored cost)
  { static int _wi=0; if(!_wi){ alloc(wct, 1u<<22); _wi=1; } }   // 4M-entry word-count table (16MB)
#endif
  assert(MEM>0);

  // update model
  assert(y==0 || y==1);
  for(int i=0;i<NM;i++) *cp[i]=nex(*cp[i], y);
#ifdef MIXER_2STAGE
  m2.update(y); for(int s=0;s<4;s++) m1[s]->update(y);
#else
  m.update(y);
#endif
#ifdef ISSECH
  for(int k=0;k<8;k++) mi[k]->update(y);
#endif

  // update context
  ++bcount;
  c0+=c0+y;
  if (c0>=256) {
    c0-=256;
    c4=c4<<8|c0;
    c8=c8<<8|(unsigned long long)c0;
    int origc0=c0;
    h[0]=c0<<8;  // order 1
    h[1]=(c4&0xffff)<<5|0x57000000;  // order 2
    h[2]=(c4<<8)*3;  // order 3
    h[3]=c4*5;  // order 4
    h[4]=h[4]*(11<<5)+c0*13&0x3fffffff;  // order 6
    h[6]=(U32)(c8&0xffffffffffULL)*2654435761u;          // order 5
    h[7]=((U32)c8 ^ (U32)(c8>>32))*2246822519u;          // order 8
    h[9]=(U32)(c8&0xffffffffffffffULL)*2654435761u;       // order 7
    h[8]=(c4 & 0x00ff00ff)*2654435761u;   // sparse {1,3} (paq8 text set)
    h[10]=(c4 & 0xff0000ff)*2246822519u;  // sparse {1,4}
    h[11]=(c4 & 0x0000ff00)*3266489917u;  // sparse {2}
    { int low=(c0>=97&&c0<=122); if(c0>=65&&c0<=90){c0+=32;low=1;}   // word stack
      if(low){ wordh=(wordh+c0)*(7<<3);
#ifdef MORE
        suf=(suf<<8)|(U32)c0;            // rolling word-suffix letters (morphology)
#endif
      }
      else {
#ifdef MORE
        suf=0;
#endif
        if(wordh){ pword=cword; cword=wordh;
#ifdef DICT
               pfreq=wfreq;                                                    // online word-freq update
               { U32 wh=((cword*2654435761u)>>10)&((1u<<22)-1); int c=wct[wh], l=0;
                 while(c>0){c>>=1;l++;} wfreq=l; if(wct[wh]<0x3fffffff) wct[wh]++; }
#endif
             } wordh=0; } }
    h[5]=wordh;                                            // current partial word
    h[12]=(cword*0x9E3779B1u)^(wordh*2654435761u);         // word BIGRAM
    h[13]=(cword*0x9E3779B1u)^(pword*40503u)^wordh;        // word TRIGRAM
    h[14]=cword*2246822519u;                               // prev word -> predict next token
#ifdef DICT
    h[19+XPN+NWIKI+NIDNUM+0]=((U32)wfreq*2654435761u)^(wordh*40503u);          // last-word freq-class x partial word
    h[19+XPN+NWIKI+NIDNUM+1]=((U32)wfreq*0x9E3779B1u)^((U32)pfreq*40503u)^(U32)(c4&0xff);  // freq bigram x lastbyte
#endif
#ifdef MORE
    hlong=(hlong+(U32)origc0+1)*2654435761u;                                   // long rolling-order context
    h[19+XPN+NWIKI+NIDNUM+NDICT+0]=(suf&0xffff)*2654435761u;                    // word-suffix (2 letters / morphology)
    h[19+XPN+NWIKI+NIDNUM+NDICT+1]=(suf&0xffffff)*40503u;                       // word-suffix (3 letters)
    h[19+XPN+NWIKI+NIDNUM+NDICT+2]=((c4>>8)&0xffff)*3266489917u;                // sparse {2,3} (skip immediate byte)
    h[19+XPN+NWIKI+NIDNUM+NDICT+3]=hlong;                                       // rolling higher-order
    h[19+XPN+NWIKI+NIDNUM+NDICT+4]=pword*2246822519u;                           // prev-prev word -> next token
#endif
    { int cls=(origc0>='0'&&origc0<='9')?'0':(origc0>='A'&&origc0<='Z')?'A':(origc0>='a'&&origc0<='z')?'a':(origc0>=128)?128:origc0;
      if((int)(grp&0xff)!=cls) grp=(grp<<8)|(U32)cls; }    // char-class skeleton
    h[15]=(grp*0x9E3779B1u)^(U32)(c4&0xff);
    { int wu=(origc0>=65&&origc0<=90), wl=wu||(origc0>=97&&origc0<=122);  // case as context
      caseh=(caseh<<1)|(U32)wu; wpos=wl?wpos+1:0; }
    h[16]=((caseh&0xff)*256+(U32)(wpos<7?wpos:7))*2654435761u;
    if(origc0=='\n'){ line1=line0; line0=0; col=0; }                      // line/column
    else { col++; line0=(line0+(U32)origc0)*0x9E3779B1u; }
    h[17]=((U32)(col<256?col:255)*2654435761u)^(U32)(c4&0xff);
    switch(origc0){case '(':ic+=31;break;case ')':ic-=31;break;case '[':ic+=11;break;  // nest depth
      case ']':ic-=11;break;case '{':ic+=17;break;case '}':ic-=17;break;
      case '<':ic+=23;break;case '>':ic-=23;break;case '\n':ic=0;break;}
    h[18]=((U32)(ic&0x3ff)*2654435761u)^(U32)(c4&0xff);
#if XPN>=1
    { int cc=origc0, pb=(c4>>8)&0xff;                                       // wiki structural mode
      if(cc=='<') xmlm=1; else if(xmlm==1&&cc=='>') xmlm=0;
      else if(pb=='['&&cc=='[') xmlm=2; else if(xmlm==2&&pb==']'&&cc==']') xmlm=0;
      else if(pb=='{'&&cc=='{') xmlm=3; else if(xmlm==3&&pb=='}'&&cc=='}') xmlm=0;
      else if(cc=='\n'){ if(xmlm==4) xmlm=0; }
      else if(col<=1&&cc=='=') xmlm=4; }
    h[19]=((U32)xmlm*2654435761u)^(wordh*40503u)^(U32)(c4&0xff); // wikimode x word (novel)
#endif
#if XPN>=2
    h[20]=(wordh*0x9E3779B1u)^(grp*2654435761u);          // word x char-class
#endif
#if XPN>=3
    h[21]=((caseh&0xff)*2246822519u)^(wordh*0x9E3779B1u);  // case x word
#endif
#ifdef WIKI
    { int cc=origc0, pb=(c4>>8)&0xff;                                       // rich wiki/xml state
      if(cc=='<'){ intag=1; tagacc=0; tagclose=0; }
      else if(intag){ if(cc=='/'&&pb=='<') tagclose=1;
        else if((cc>=97&&cc<=122)||(cc>=65&&cc<=90)) tagacc=tagacc*33u+(U32)(cc|32);
        if(cc=='>'){ intag=0; container=(tagclose||pb=='/')?0:tagacc; } }
      if(pb=='['&&cc=='[') wmode2=2; else if(wmode2==2&&pb==']'&&cc==']') wmode2=0;
      else if(pb=='{'&&cc=='{') wmode2=3; else if(wmode2==3&&pb=='}'&&cc=='}') wmode2=0;
      else if(pb=='{'&&cc=='|') wmode2=5; else if(wmode2==5&&pb=='|'&&cc=='}') wmode2=0;
      else if(cc=='<') wmode2=1; else if(wmode2==1&&cc=='>') wmode2=0;
      else if(pb=='\n'&&cc=='=') wmode2=4; else if(wmode2==4&&cc=='\n') wmode2=0;
      if(pb=='\n') listc=(cc=='*'||cc=='#'||cc==':'||cc==';'||cc=='|'||cc=='!')?cc:0;
#ifdef IDNUM
      if(container!=prevcont){ fieldpos=0; prevcont=container; } else if(fieldpos<65535) fieldpos++;
      if(cc>=48&&cc<=57){ if(digrun<255) digrun++; } else digrun=0;
#endif
    }
    // wiki contexts, ordered by rig importance; -DWIKIN=k includes the first k
  // reordered by overnight3 isolation marginal (descending); list (harmful) last -> WIKIN=6 = pruned pack
  #if NWIKI>=1
    h[19+XPN+0]=((U32)wmode2*0x9E3779B1u)^(grp*2246822519u);                 // richmode x class   (+0.410%)
  #endif
  #if NWIKI>=2
    h[19+XPN+1]=(container*40503u)^(grp*2654435761u);                        // container x class  (+0.217%)
  #endif
  #if NWIKI>=3
    h[19+XPN+2]=(container*2654435761u)^((U32)wmode2*40503u);                // container x richmode (+0.139%)
  #endif
  #if NWIKI>=4
    h[19+XPN+3]=((U32)wmode2*2654435761u)^(wordh*40503u)^(U32)(c4&0xff);     // richmode x word    (+0.053%)
  #endif
  #if NWIKI>=5
    h[19+XPN+4]=(container*0x9E3779B1u)^(wordh*40503u);                      // container x word   (+0.008%)
  #endif
  #if NWIKI>=6
    h[19+XPN+5]=(container*2654435761u)^(U32)(c4&0xff);                      // container x lastbyte (+0.006%)
  #endif
  #if NWIKI>=7
    h[19+XPN+6]=((U32)listc*2246822519u)^(U32)(c4&0xff);                     // list marker        (-0.021%, drop)
  #endif
#endif
#ifdef IDNUM
    h[19+XPN+NWIKI+0]=(container*2654435761u)^((U32)(digrun<16?digrun:15)*40503u);              // field x digit-run
    h[19+XPN+NWIKI+1]=(container*0x9E3779B1u)^((U32)(fieldpos<64?fieldpos:63)*2246822519u);     // field x position
    h[19+XPN+NWIKI+2]=(container*40503u)^((U32)(digrun>0)*2654435761u)^(U32)(c4&0xff);          // field x is-digit x lastbyte
    h[19+XPN+NWIKI+3]=((U32)(fieldpos<32?fieldpos:31)*2654435761u)^((U32)wmode2*40503u);        // position x mode
#endif
    for(int i=1;i<NM;i++) cp[i]=t[h[i]]+1;
#ifdef RUNMAP
    { U16 rec=rt[runhash]; int pb=rec>>8, pc=rec&0xff;        // record byte that followed prev context
      if(pb==origc0){ if(pc<250) pc++; } else { pb=origc0; pc=1; }
      rt[runhash]=(U16)((pb<<8)|pc); }
    runhash=((c4&0xffffff)*2654435761u)>>8;                   // order-3 context for upcoming byte
    runhash&=(1u<<24)-1;
    { U16 rec=rt[runhash]; runpred=rec>>8; runcnt=rec&0xff; }
#endif
    c0=1;
    bcount=0;
  }
  if (bcount==4) {
    for(int i=1;i<NM;i++) cp[i]=t[h[i]+c0]+1;
  }
  else if (bcount>0) {
    int j=y+1<<(bcount&3)-1;
    for(int i=1;i<NM;i++) cp[i]+=j;
  }
  cp[0]=t0+h[0]+c0;

  // predict — collect all model inputs into in[], then feed the mixer(s)
  int in[NIN]; int nin=0;
#ifdef MATCH3
  int mn=0; int len=mm.p3(y, &in[nin], &mn); nin += mn;   // full match cascade: writes NMATCH inputs
#else
  int matchpr; int len=mm.p(y, &matchpr);
  in[nin++]=matchpr;
#endif
  int order=0;
  if (len==0) {
    if (*cp[4]) ++order;
    if (*cp[3]) ++order;
    if (*cp[2]) ++order;
    if (*cp[1]) ++order;
  }
  else order=5+(len>=8)+(len>=12)+(len>=16)+(len>=32);
  for(int i=0;i<NM;i++){ int pp=sm[i].p(y, *cp[i]); in[nin++]=stretch(pp);
#ifdef ISSE2
    in[nin++]=(pp-2048);   // 2nd input: linear residual view (orthogonal to log/stretch)
#endif
  }
#ifdef RUNMAP
  { int rmctx=0, bc=(c0<2)?0:(c0<4)?1:(c0<8)?2:(c0<16)?3:(c0<32)?4:(c0<64)?5:(c0<128)?6:7;
    if(runcnt>0 && ((runpred+256)>>(8-bc))==c0){
      int predbit=(runpred>>(7-bc))&1;
      int cb=(runcnt<16)?runcnt:(16+((runcnt>>3)<7?(runcnt>>3):7));
      rmctx=1+(predbit<<5)+cb; }                 // predicted bit + run-length bucket, <128
    in[nin++]=stretch(sm_run.p(y, rmctx)); }
#endif
#ifdef ISSECH
  { static const int ORDK[8]={0,1,2,3,6,4,9,7};   // order 1,2,3,4,5,6,7,8 context slots, increasing
    int pc=2048;
    for(int k=0;k<8;k++){ int st=*cp[ORDK[k]];
      int sp=smc[k].p(y, st);
      mi[k]->add(stretch(pc)); mi[k]->add(stretch(sp)); mi[k]->set(st); pc=mi[k]->p(); }
    in[nin++]=stretch(pc); }
#endif
  int lc=grp&0xff, cc=(lc=='a')?0:(lc=='A')?1:(lc=='0')?2:(lc==' ')?3:4;
#ifdef MIXER_2STAGE
  for(int s=0;s<4;s++){ for(int i=0;i<nin;i++) m1[s]->add(in[i]); }
  m1[0]->set(order + 10*(h[0]>>13) + 80*cc);                            // proven gate
  m1[1]->set(bcount + 8*cc + 40*((c4&0xe0)>>5));                        // bpos x class x prevbits
  m1[2]->set(((U32)wmode2&7) + 8*((U32)container&0xf) + 128*((U32)cc&3)); // structural (field/mode)
  m1[3]->set(order + 10*(len>0?(len<15?len:15):0) + 160*(bcount&1));    // match-state aware
  int pr1[4]; for(int s=0;s<4;s++) pr1[s]=m1[s]->p();
  for(int s=0;s<4;s++) m2.add(stretch(pr1[s]));
#ifdef S2RICH
  m2.set(order + 10*cc);   // stage-2 specializes per (order, char-class)
#else
  m2.set(order);
#endif
  pr=m2.p();
#else
  for(int i=0;i<nin;i++) m.add(in[i]);
  m.set(order + 10*(h[0]>>13) + 80*cc);   // mixer gated by char-class
  pr=m.p();
#endif
  pr=pr+3*a1.pp(y, pr, c0)>>2;
  pr=pr+3*a2.pp(y, pr, c0^h[0]>>2)>>2;
  pr=pr+3*a3.pp(y, pr, cword&0xfff)>>2;        // 3rd SSE: word context
#ifdef SSE45
  { int lb=(len<16)?len:(16+((len>>2)<12?(len>>2):12));
    pr=pr+3*a4.pp(y, pr, ((lb<<3)|(c0&7))&0xff)>>2; }            // 4th SSE: match-state
  pr=pr+3*a5.pp(y, pr, ((container^((U32)wmode2<<6)))&0xfff)>>2; // 5th SSE: structural
#endif
}

//////////////////////////// Encoder ////////////////////////////

// An Encoder does arithmetic encoding.  Methods:
// Encoder(COMPRESS, f) creates encoder for compression to archive f, which
//     must be open past any header for writing in binary mode.
// Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
//     which must be open past any header for reading in binary mode.
// code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
// code() in DECOMPRESS mode returns the next decompressed bit from file f.
// compress(c) in COMPRESS mode compresses one byte.
// decompress() in DECOMPRESS mode decompresses and returns one byte.
// flush() should be called exactly once after compression is done and
//     before closing f.  It does nothing in DECOMPRESS mode.

int autonomous_mood = 0;

typedef enum {COMPRESS, DECOMPRESS} Mode;
class Encoder {
private:
  Predictor predictor;
  const Mode mode;       // Compress or decompress?
  FILE* archive;         // Compressed data file
  U32 x1, x2;            // Range, initially [0, 1), scaled by 2^32
  U32 x;                 // Decompress mode: last 4 input bytes of archive

  // Compress bit y or return decompressed bit
  int code(int y=0) {
    int p=predictor.p();
    assert(p>=0 && p<4096);
    if(autonomousmode) {

        if(autonomous_mood) {
            --autonomous_mood;
            predictor.update(rand()>RAND_MAX/2);
        } else {
            predictor.update(p>2048);
        }

        if (rand()<RAND_MAX/2000) {
            autonomous_mood = rand()*100LL/RAND_MAX;
        }

        return p>2048;
    }
    p+=p<2048;
    U32 xmid=x1 + (x2-x1>>12)*p + ((x2-x1&0xfff)*p>>12);
    /*if(fuzzmode && rand()<RAND_MAX/1000000) {
        xmid += rand()*2LL/RAND_MAX-1;
        if(xmid<x1)xmid=x1;
        if(xmid>=x2)xmid=x2-1;
    } */
    assert(xmid>=x1 && xmid<x2);
    if (mode==DECOMPRESS) y=x<=xmid;
    y ? (x2=xmid) : (x1=xmid+1);
    if(fuzzmode && rand()<RAND_MAX/10000) {
        p = 3*p/5 + 2*(y?4096:0)/5;
        predictor.update(p>2048);
    } else {
        predictor.update(y);
    }
    while (((x1^x2)&0xff000000)==0) {  // pass equal leading bytes of range
      if (mode==COMPRESS) putc(x2>>24, archive);
      x1<<=8;
      x2=(x2<<8)+255;
      if (mode==DECOMPRESS) x=(x<<8)+(getc(archive)&255);  // EOF is OK
    }
    if (guessedmode) {
        return p>2048;
    }
    return y;
  }

public:
  Encoder(Mode m, FILE* f);
  void flush();  // call this when compression is finished

  // Compress one byte
  void compress(int c) {
    assert(mode==COMPRESS);
    for (int i=7; i>=0; --i)
      code((c>>i)&1);
  }

  // Decompress and return one byte
  int decompress() {
    int c=0;
    for (int i=0; i<8; ++i)
      c+=c+code();
    return c;
  }
};

Encoder::Encoder(Mode m, FILE* f):
    mode(m), archive(f), x1(0), x2(0xffffffff), x(0) {
  if (mode==DECOMPRESS) {  // x = first 4 bytes of archive
    for (int i=0; i<4; ++i)
      x=(x<<8)+(getc(archive)&255);
  }
}

void Encoder::flush() {
  if (mode==COMPRESS)
    putc(x1>>24, archive);  // Flush first unequal byte of range
}


//////////////////////////// User Interface ////////////////////////////


Encoder *usr1_encoder;
FILE* usr1_out;
int signal_workaround=0;
void usr1 (int _) {
    signal_workaround=1;
    //usr1_encoder->flush();
    fflush(usr1_out);
}


int streammode=0;

int main(int argc, char **argv) {

  // (win32: signal setup removed)
  // Check arguments
  if (argc!=4 || !isdigit(argv[1][0]) && argv[1][0]<'a' && argv[1][0]>'z') {
    printf(
      "lpaq1 file compressor (C) 2007, Matt Mahoney\n"
      "Licensed under GPL, http://www.gnu.org/copyleft/gpl.html\n"
      "\n"
      "To compress:   lpaq1 N input output  (N=0..9, uses 3+3*2^N MB)\n"
      "To decompress: lpaq1 d input output  (needs same memory)\n"
      "Stream decompress: lpaq1 s input output  (unknown filesize, puts trailer)\n"
      "Fuzz decompress: lpaq1 f input output  (intentionally misdecompress data)\n"
      "Autonomous mode: lpaq1 a input output  (keep \"decompressing\" forever based on predictor after EOF)\n"
      "Guessed mode: lpaq1 g input output  (Output only predicted bits, but use actual info for feeding predictor)\n");
    return 1;
  }

  if (argv[1][0]=='f') {
    fuzzmode=1;
    streammode=1;
  }
  if (argv[1][0]=='a') {
    streammode=1;
  }
  if (argv[1][0]=='s') {
    streammode=1;
  }
  if (argv[1][0]=='g') {
    guessedmode=1;
    streammode=1;
  }

  // Get start time
  clock_t start=clock();

  // Open input file
  FILE *in = NULL;
  if(!strcmp(argv[2],"-")) {
      in=stdin;
  } else  {
      in=fopen(argv[2], "rb");
  }
  if (!in) perror(argv[2]), exit(1);
  FILE *out=NULL;
  if (!strcmp(argv[3],"-")) {
    out=stdout;
  } else {
    out=fopen(argv[3], "wb");
  }
  if (!out) perror(argv[3]), exit(1);

  // Compress
  if (isdigit(argv[1][0])) {
    MEM=1<<(argv[1][0]-'0'+20);

    // Encode header: version 1, memory option, file size
    fseek(in, 0, SEEK_END);
    long size=ftell(in);
    //if (size<0 || size>=0x7FFFFFFF) quit("input file too big");
    fseek(in, 0, SEEK_SET);
    fprintf(out, "pQ%c%c%c%c%c%c", 1, argv[1][0], 
      size>>24, size>>16, size>>8, size);

    // Compress
    Encoder e(COMPRESS, out);
    usr1_encoder = &e;
    usr1_out = out;
    int c;
    for (;;) {
      c=getc(in);
      if(c==EOF) {
          if(!signal_workaround)break;
          continue;
      }
      e.compress(c);
      signal_workaround = 0;
    }
    e.flush();
  }

  // Decompress
  else {

    // Check header version, get memory option, file size
    if (getc(in)!='p' || getc(in)!='Q' || getc(in)!=1)
      quit("Not a lpaq1 file");
    MEM=getc(in);
    if (MEM<'0' || MEM>'9') quit("Bad memory option (not 0..9)");
    MEM=1<<(MEM-'0'+20);
    long size=getc(in)<<24;
    size|=getc(in)<<16;
    size|=getc(in)<<8;
    size|=getc(in);
    if (!streammode) {
        if (size<0) quit("Bad file size");
    }

    // Decompress
    Encoder e(DECOMPRESS, in);
    usr1_encoder = &e;
    usr1_out = out;
    while (size-->0 || streammode) {
      putc(e.decompress(), out);
      if(streammode && feof(in) && !signal_workaround)break;
      signal_workaround=0;
    }
    if(argv[1][0]=='a') {
        autonomousmode=1;
        for(;;) {
            putc(e.decompress(), out);
        }
    }
    if(streammode) {
        /* put trailer */
        int i;
        for(i=0; i<256; ++i) {
            putc(e.decompress(), out);
        }
    }
  }

  // Report result
  assert(in);
  assert(out);
  fprintf(stderr, "%ld -> %ld in %1.2f sec. using %d MB memory.\n", 
    ftell(in), ftell(out), double(clock()-start)/CLOCKS_PER_SEC, 
    3+(MEM>>20)*3);

  return 0;
}


