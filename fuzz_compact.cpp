// Fast, decode-only fuzz of the compact/legacy path opened by the 2026-08-07 size-gate fix.
// Feeds uZIP-magic (B5 49 5A 50) + version + random tiny streams straight to decompress().
// Must never crash. Seeded, reproducible. usage: fuzz_compact <iters> <seed>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"
static uint64_t st=0x1234;
static inline uint32_t rnd(){ st=st*6364136223846793005ULL+1442695040888963407ULL; return (uint32_t)(st>>33); }
int main(int argc,char**argv){
  uint64_t N=argc>1?strtoull(argv[1],0,10):5000000ULL; st=argc>2?strtoull(argv[2],0,0):0x1234ULL;
  uint64_t ok=0,ne=0;
  for(uint64_t i=0;i<N;i++){
    st ^= i*0x9E3779B97F4A7C15ULL; st=st*6364136223846793005ULL+1442695040888963407ULL;
    std::vector<uint8_t> s;
    s.push_back(0xB5); s.push_back(0x49); s.push_back(0x5A); s.push_back(0x50);
    s.push_back((uint8_t)((rnd()&1)?0x02:0x01));                 // version compact/legacy
    uint32_t n = (rnd()%3? rnd()%28 : rnd()%4096);              // mostly tiny (5..33B), some larger
    for(uint32_t k=0;k<n;k++) s.push_back((uint8_t)rnd());
    // No per-iter file I/O: streams are deterministic from (seed,iter); a crash localizes to the
    // last printed 1M window and is reconstructable by re-running that range with the same seed.
    try { auto d=mzip::decompress(s.data(),s.size()); if(d.empty())ne++; else ok++; } catch(...){}
    if((i%1000000)==0){ printf("iter=%llu ok=%llu empty=%llu\n",(unsigned long long)i,(unsigned long long)ok,(unsigned long long)ne); fflush(stdout);}
  }
  printf("FUZZCOMPACT DONE N=%llu (no crash)\n",(unsigned long long)N);
  return 0;
}
