// fuzz_decode.cpp -- untrusted-stream robustness fuzzer for mzip::decompress.
// Feeds MUTATED well-formed streams, PURE-RANDOM bytes, and MAGIC-PREFIXED random bytes to
// decompress() and asserts it never crashes / hangs / OOMs. (Losslessness is not the goal here --
// mutated streams need not roundtrip; the goal is graceful failure on hostile input.) A SEGV halts
// the process (a finding) with the crashing stream saved to fuzzd_cur.bin. Seeded LCG (reproducible).
//   usage: fuzz_decode <iterations=1000000> <seed=0xD00D>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"

static uint64_t g_st = 0xD00DULL;
static inline uint32_t rnd(){ g_st = g_st*6364136223846793005ULL + 1442695040888963407ULL; return (uint32_t)(g_st>>33); }
static inline uint32_t rr(uint32_t n){ return n ? (rnd()%n) : 0; }
static void put(std::vector<uint8_t>&v,const char*s){ while(*s) v.push_back((uint8_t)*s++); }
static void puti(std::vector<uint8_t>&v,long long x){ char b[32]; int n=snprintf(b,sizeof b,"%lld",x); for(int i=0;i<n;i++) v.push_back((uint8_t)b[i]); }

// a compact spread of base inputs so mutated streams cover many encoder formats
static std::vector<uint8_t> base_input(){
  switch(rr(6)){
    case 0: { size_t n=rr(4096); std::vector<uint8_t> v(n); for(size_t i=0;i<n;i++) v[i]=(uint8_t)rnd(); return v; }
    case 1: { std::vector<uint8_t> v; put(v,"id,a,b\n"); uint32_t r=1+rr(200); for(uint32_t i=0;i<r;i++){ puti(v,1000+i); v.push_back(','); puti(v,rr(1000000)); put(v,",v"); puti(v,rr(50)); v.push_back('\n'); } return v; }
    case 2: { std::vector<uint8_t> v; put(v,"INSERT INTO t (id,a) VALUES "); uint32_t r=4+rr(200); for(uint32_t i=0;i<r;i++){ if(i) v.push_back(','); v.push_back('('); puti(v,1+i); put(v,",'x"); puti(v,rr(99)); put(v,"')"); } put(v,";\n"); return v; }
    case 3: { std::vector<uint8_t> v; uint32_t r=4+rr(200); long long t=971211336; for(uint32_t i=0;i<r;i++){ t+=rr(4); char b[128]; int n=snprintf(b,sizeof b,"1.2.3.%u - - [10/Oct/2000:13:%02lld:%02lld -0700] \"GET / HTTP/1.1\" 200 %u\n",rr(255),(t/60)%60,t%60,rr(9999)); for(int k=0;k<n;k++) v.push_back((uint8_t)b[k]); } return v; }
    case 4: { size_t n=256+rr(4096); std::vector<uint8_t> v(n); for(size_t i=0;i<n;i++) v[i]=(uint8_t)rnd(); for(size_t i=0;i+5<n;i+=8+rr(30)) v[i]=0xE8; if(rr(2)){v[0]='M';v[1]='Z';} return v; }
    default:{ uint32_t nel=8+rr(1000); std::vector<uint8_t> v(nel*4); for(uint32_t e=0;e<nel;e++){ float f=(float)(e*0.01); memcpy(&v[e*4],&f,4);} return v; }
  }
}
static void mutate(std::vector<uint8_t>& s){
  uint32_t how=rr(4);
  if(how==0 && !s.empty()){ uint32_t f=1+rr(16); for(uint32_t i=0;i<f;i++) s[rr((uint32_t)s.size())]^=(uint8_t)rnd(); }   // byte scrambles
  else if(how==1 && !s.empty()){ s.resize(rr((uint32_t)s.size())); }                                                    // truncate
  else if(how==2){ uint32_t add=rr(64); for(uint32_t i=0;i<add;i++) s.push_back((uint8_t)rnd()); }                       // extend
  else if(how==3 && s.size()>8){ size_t p=rr((uint32_t)s.size()-4); uint32_t big=0x7fffffff-rr(1000); memcpy(&s[p],&big,4); } // inject huge length
}

int main(int argc,char**argv){
  uint64_t N = argc>1 ? strtoull(argv[1],0,10) : 1000000ULL;
  uint64_t base = argc>2 ? strtoull(argv[2],0,0) : 0xD00DULL;
  uint64_t start = argc>3 ? strtoull(argv[3],0,0) : 0ULL;   // resume point (skip crashers)
  uint64_t ok=0, exc=0, empty=0, nonempty=0;
  for(uint64_t i=start;i<N;i++){
    // per-iter independent seed: any iter is directly reproducible AND skippable after a crash
    g_st = base ^ (i * 0x9E3779B97F4A7C15ULL); g_st = g_st*6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t seed=g_st;
    std::vector<uint8_t> stream;
    uint32_t kind=rr(5);
    if(kind<=1){ auto in=base_input(); stream=mzip::compress(in.data(),in.size(),(rnd()&1)?3:19,mzip::DEFAULT_BLOCK_SIZE,nullptr,(rnd()&1)?mzip::CompressionMode::BALANCED:mzip::CompressionMode::SMALL); if(kind==1) mutate(stream); }
    else if(kind==2){ size_t n=rr(2048); stream.resize(n); for(size_t k=0;k<n;k++) stream[k]=(uint8_t)rnd(); }             // pure random
    else if(rr(3)==0){ // 4-byte uZIP magic (B5 49 5A 50) + version -> exercises the compact/legacy
      // decode path, incl. the TINY (5-16B) streams the 2026-08-07 size-gate fix now admits.
      stream.push_back(0xB5); stream.push_back(0x49); stream.push_back(0x5A); stream.push_back(0x50);
      stream.push_back((uint8_t)((rnd()&1)?0x02:0x01)); // VERSION_COMPACT / legacy
      size_t n = (rr(2)? rr(24) : rr(2048));            // bias small to hit the newly-opened floor
      for(size_t k=0;k<n;k++) stream.push_back((uint8_t)rnd());
    }
    else { // 2-byte magic-prefixed random (stress the format dispatchers)
      static const char* M[]={"MT","MQ","ML","MB","MS","MU","MC","BT","CL","BG","MZ"};
      const char* mg=M[rr(11)]; stream.push_back(mg[0]); stream.push_back(mg[1]);
      size_t n = (rr(4)==0? rr(24) : rr(2048));         // occasionally tiny
      for(size_t k=0;k<n;k++) stream.push_back((uint8_t)rnd());
    }
    { FILE* cf=fopen("fuzzd_cur.bin","wb"); if(cf){ fwrite(stream.data(),1,stream.size(),cf); fclose(cf);}
      FILE* mf=fopen("fuzzd_cur.txt","w"); if(mf){ fprintf(mf,"iter=%llu base=0x%llx kind=%u size=%zu\n",(unsigned long long)i,(unsigned long long)base,kind,stream.size()); fclose(mf);} }
    try { auto d = mzip::decompress(stream.data(), stream.size()); if(d.empty()) empty++; else nonempty++; }
    catch(const std::exception& e){ exc++; }
    ok++;
    if((i%50000)==0){ printf("iter=%llu ok=%llu exc=%llu empty=%llu nonempty=%llu\n",(unsigned long long)i,(unsigned long long)ok,(unsigned long long)exc,(unsigned long long)empty,(unsigned long long)nonempty); fflush(stdout); (void)seed; }
  }
  printf("FUZZDECODE DONE N=%llu ok=%llu exc=%llu (no crash)\n",(unsigned long long)N,(unsigned long long)ok,(unsigned long long)exc);
  return 0;
}
