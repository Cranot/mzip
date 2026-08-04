// Does the provable cap actually fire, and what does it save?
// Compares bwt5::compress(data,n) against bwt5::compress(data,n,cap) on one block.
// Output MUST be identical whenever the uncapped result is < cap; when it is >= cap
// the caller discards it either way, so any difference there is legitimate.
#include "bwt_compress_v5.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
using clk = std::chrono::steady_clock;
static double ms(clk::time_point a, clk::time_point b){ return std::chrono::duration<double,std::milli>(b-a).count(); }
int main(int c,char**v){
  if(c<3){printf("usage: bwt5_cap_probe <file> <cap>\n");return 1;}
  size_t cap=(size_t)atoll(v[2]);
  FILE*f=fopen(v[1],"rb"); if(!f)return 1;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n){fclose(f);return 1;} fclose(f);
  double tu=1e18, tc=1e18; size_t su=0, sc=0;
  for(int r=0;r<3;r++){
    auto t0=clk::now(); auto u=bwt5::compress(b.data(),(size_t)n);      auto t1=clk::now();
    auto t2=clk::now(); auto k=bwt5::compress(b.data(),(size_t)n,cap);  auto t3=clk::now();
    double du=ms(t0,t1), dc=ms(t2,t3);
    if(du<tu)tu=du; if(dc<tc)tc=dc; su=u.size(); sc=k.size();
  }
  printf("%-34s n=%-8ld cap=%-9zu | uncapped %8.1f ms -> %8zu B | capped %8.1f ms -> %8zu B | %+6.1f%%  %s\n",
         v[1], n, cap, tu, su, tc, sc, 100.0*(tc-tu)/tu,
         (su==sc) ? "same result" : (su>=cap ? "differs, but uncapped >= cap so caller discards both" : "*** DIFFERS BELOW CAP - BUG ***"));
  return 0;
}
