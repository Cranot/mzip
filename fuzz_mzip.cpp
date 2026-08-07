// fuzz_mzip.cpp -- losslessness + robustness fuzzer for mzip's encoders.
// Each iteration: generate a diverse/mutated input, compress -> decompress -> assert cmp==input,
// under a seeded LCG (reproducible). Stops (writes fuzz_fail.bin + seed) on any lossless failure;
// logs exceptions/robustness issues and continues. A SEGV would kill the process (also a finding).
//   usage: fuzz_mzip <iterations=1000000> <seed=0x12345678>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"

static uint64_t g_st = 0x12345678ULL;
static inline uint32_t rnd(){ g_st = g_st*6364136223846793005ULL + 1442695040888963407ULL; return (uint32_t)(g_st>>33); }
static inline uint32_t rr(uint32_t n){ return n ? (rnd()%n) : 0; }

static void put(std::vector<uint8_t>&v,const char*s){ while(*s) v.push_back((uint8_t)*s++); }
static void puti(std::vector<uint8_t>&v,long long x){ char b[32]; int n=snprintf(b,sizeof b,"%lld",x); for(int i=0;i<n;i++) v.push_back((uint8_t)b[i]); }

// ---- generators (each returns a fresh input) ----
static std::vector<uint8_t> gen_random(){ size_t n=rr(8192); std::vector<uint8_t> v(n); for(size_t i=0;i<n;i++) v[i]=(uint8_t)rnd(); return v; }
static std::vector<uint8_t> gen_boundary(){ static const size_t S[]={0,1,2,3,4,5,10,63,64,65,255,256,257,4095,4096,4097}; size_t n=S[rr(16)]; std::vector<uint8_t> v(n); uint8_t f=(uint8_t)rnd(); for(size_t i=0;i<n;i++) v[i]=(rr(4)?f:(uint8_t)rnd()); return v; }
static std::vector<uint8_t> gen_csv(){ std::vector<uint8_t> v; uint32_t cols=2+rr(8), rows=1+rr(400); bool tab=rr(2);
  put(v,"id"); for(uint32_t c=1;c<cols;c++){ v.push_back(tab?'\t':','); put(v,"c"); puti(v,c); } v.push_back('\n');
  long long base=rr(100000); int step=1+rr(5);
  for(uint32_t r=0;r<rows;r++){ puti(v, base+(long long)r*step); for(uint32_t c=1;c<cols;c++){ v.push_back(tab?'\t':','); if(rr(2)) puti(v, rr(1000000)); else { put(v,"v"); puti(v, rr(50)); } } v.push_back('\n'); } return v; }
static std::vector<uint8_t> gen_sql(){ std::vector<uint8_t> v; put(v,"-- dump\nINSERT INTO t (id,a,b) VALUES "); uint32_t rows=4+rr(300);
  bool multi=rr(2);
  for(uint32_t r=0;r<rows;r++){ if(r){ if(multi) v.push_back(','); else put(v,";\nINSERT INTO t (id,a,b) VALUES "); }
    v.push_back('('); puti(v, 1+r); put(v,",'s"); puti(v, rr(100)); put(v,"',"); puti(v, rr(100000)); v.push_back(')'); }
  put(v,";\n"); return v; }
static std::vector<uint8_t> gen_clf(){ std::vector<uint8_t> v; uint32_t rows=4+rr(400); long long t=971211336+rr(100000); static const char* MON="Jan";
  for(uint32_t r=0;r<rows;r++){ t += (int)rr(5) - (rr(6)?0:2); // mostly forward, occasional backward
    char b[160]; int hh=(int)((t/3600)%24), mm=(int)((t/60)%60), ss=(int)(t%60);
    int n=snprintf(b,sizeof b,"10.0.0.%u - - [10/%s/2000:%02d:%02d:%02d -0700] \"GET /p/%u HTTP/1.1\" 200 %u\n",rr(255),MON,hh<0?0:hh,mm<0?0:mm,ss<0?0:ss,rr(50),100+rr(5000));
    for(int i=0;i<n;i++) v.push_back((uint8_t)b[i]); } return v; }
static std::vector<uint8_t> gen_iso(){ std::vector<uint8_t> v; uint32_t rows=4+rr(400); long long t=1704067200+rr(100000); int w=rr(2)?3:0; char sep=rr(2)?'T':' ';
  for(uint32_t r=0;r<rows;r++){ t += rr(4); long long d=t/86400, tod=t%86400; long long H=tod/3600,M=(tod%3600)/60,S=tod%60;
    // fixed year for simplicity 2024-01-.. approx (not calendar-exact but the parser+gate handle it)
    char b[80]; int n; if(w) n=snprintf(b,sizeof b,"2024-01-%02lld%c%02lld:%02lld:%02lld.%03u INFO msg%u\n",1+(d%28),sep,H,M,S,rr(1000),rr(100));
    else n=snprintf(b,sizeof b,"2024-01-%02lld%c%02lld:%02lld:%02lld INFO msg%u\n",1+(d%28),sep,H,M,S,rr(100));
    for(int i=0;i<n;i++) v.push_back((uint8_t)b[i]); } return v; }
static std::vector<uint8_t> gen_binary(){ size_t n=256+rr(8192); std::vector<uint8_t> v(n); for(size_t i=0;i<n;i++) v[i]=(uint8_t)rnd(); for(size_t i=0;i+5<n;i+=8+rr(40)){ v[i]=rr(2)?0xE8:0xE9; } if(rr(2)){ v[0]='M'; v[1]='Z'; } return v; }
static std::vector<uint8_t> gen_numeric(){ uint32_t nel=8+rr(2000); int W=rr(2)?4:8; std::vector<uint8_t> v(nel*W); double base=rr(1000); for(uint32_t e=0;e<nel;e++){ double val=base + e*0.01 + (rr(100)*0.001); if(W==8){ memcpy(&v[e*8],&val,8);} else { float f=(float)val; memcpy(&v[e*4],&f,4);} } return v; }
static std::vector<uint8_t> gen_text(){ static const char* W[]={"the","of","and","to","in","a","is","that","it","for","was","on","are","as"}; size_t target=rr(8192); std::vector<uint8_t> v; while(v.size()<target){ put(v,W[rr(14)]); v.push_back(rr(8)?' ':'\n'); } return v; }

std::vector<uint8_t> generate2();  // fwd decl (non-recursive concat generator)
static std::vector<uint8_t> generate(){
  std::vector<uint8_t> v;
  switch(rr(9)){
    case 0: v=gen_random(); break; case 1: v=gen_boundary(); break; case 2: v=gen_csv(); break;
    case 3: v=gen_sql(); break; case 4: v=gen_clf(); break; case 5: v=gen_iso(); break;
    case 6: v=gen_binary(); break; case 7: v=gen_numeric(); break; default: v=gen_text(); break;
  }
  // mutations
  uint32_t m=rr(5);
  if(m==1 && !v.empty()){ size_t k=rr((uint32_t)v.size()); v.resize(k); }                       // truncate
  else if(m==2 && !v.empty()){ uint32_t f=1+rr(8); for(uint32_t i=0;i<f;i++) v[rr((uint32_t)v.size())]^=(uint8_t)(1<<rr(8)); } // bit-flips
  else if(m==3){ auto w=generate2(); v.insert(v.end(), w.begin(), w.end()); }                     // concat (bounded via generate2)
  return v;
}
// non-recursive second generator for concat (avoid deep recursion)
std::vector<uint8_t> generate2(){ switch(rr(9)){ case 0: return gen_random(); case 1: return gen_boundary(); case 2: return gen_csv(); case 3: return gen_sql(); case 4: return gen_clf(); case 5: return gen_iso(); case 6: return gen_binary(); case 7: return gen_numeric(); default: return gen_text(); } }

int main(int argc,char**argv){
  uint64_t N = argc>1 ? strtoull(argv[1],0,10) : 1000000ULL;
  g_st = argc>2 ? strtoull(argv[2],0,0) : 0x12345678ULL;
  uint64_t ok=0, exc=0;
  for(uint64_t i=0;i<N;i++){
    uint64_t seed=g_st;
    std::vector<uint8_t> in=generate();
    std::vector<uint8_t> c, d;
    int lvl = (rnd()%10)? 3 : 19;
    mzip::CompressionMode mode = (rnd()%10)? mzip::CompressionMode::BALANCED : mzip::CompressionMode::SMALL;
    // repro capture: overwrite the current input + meta (incl lvl/mode) each iter so a SEGV leaves the crasher on disk
    { FILE* cf=fopen("fuzz_cur.bin","wb"); if(cf){ fwrite(in.data(),1,in.size(),cf); fclose(cf); }
      FILE* mf=fopen("fuzz_cur.txt","w"); if(mf){ fprintf(mf,"iter=%llu seed=0x%llx size=%zu lvl=%d mode=%d\n",(unsigned long long)i,(unsigned long long)seed,in.size(),lvl,(int)mode); fclose(mf); } }
    try {
      c = mzip::compress(in.data(), in.size(), lvl, mzip::DEFAULT_BLOCK_SIZE, nullptr, mode);
      d = mzip::decompress(c.data(), c.size());
    } catch(const std::exception& e){
      printf("EXCEPTION iter=%llu seed=0x%llx insize=%zu: %s\n",(unsigned long long)i,(unsigned long long)seed,in.size(),e.what());
      { FILE* ef=fopen("fuzz_exc.bin","wb"); if(ef){ fwrite(in.data(),1,in.size(),ef); fclose(ef);} }  // capture (overwrite) the last throwing input
      exc++; continue;  // keep hunting: LOSSY (silent corruption) is the real target, not caught exceptions
    }
    if(d.size()!=in.size() || (in.size() && memcmp(d.data(),in.data(),in.size())!=0)){
      printf("*** LOSSY iter=%llu seed=0x%llx insize=%zu outsize=%zu magic=%02x%02x lvl=%d mode=%d ***\n",
        (unsigned long long)i,(unsigned long long)seed,in.size(),d.size(), c.size()>0?c[0]:0, c.size()>1?c[1]:0, lvl,(int)mode);
      FILE* f=fopen("fuzz_fail.bin","wb"); if(f){ fwrite(in.data(),1,in.size(),f); fclose(f); }
      printf("FUZZ FAIL -- wrote fuzz_fail.bin\n"); return 1;
    }
    ok++;
    if((i%25000)==0){ printf("iter=%llu ok=%llu exc=%llu\n",(unsigned long long)i,(unsigned long long)ok,(unsigned long long)exc); fflush(stdout); }
  }
  printf("FUZZ DONE N=%llu ok=%llu exceptions=%llu (0 lossy)\n",(unsigned long long)N,(unsigned long long)ok,(unsigned long long)exc);
  return 0;
}
