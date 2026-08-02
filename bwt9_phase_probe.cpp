// Phase probe: how is bwt9::compress's time actually split?
// bwt9 = (bwt5|bwt8) + cmbk::compress_bwt, and BOTH build a libsais suffix array
// of the SAME input. This measures whether that duplicated BWT is worth sharing.
#include "bwt_compress_v9.hpp"
#include "libsais.h"
#include <cstdio>
#include <vector>
#include <chrono>
using clk = std::chrono::steady_clock;
static double ms(clk::time_point a, clk::time_point b){
  return std::chrono::duration<double,std::milli>(b-a).count();
}
int main(int c,char**v){
  if(c<2){printf("usage: bwt9_phase_probe <file>\n");return 1;}
  FILE*f=fopen(v[1],"rb"); if(!f)return 1;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n)return 1; fclose(f);

  // libsais alone, exactly as both callers invoke it
  double t_sais=1e18;
  for(int r=0;r<3;r++){
    std::vector<uint8_t> bwt(n); std::vector<int32_t> A(n+1);
    auto t0=clk::now();
    libsais_bwt(b.data(), bwt.data(), A.data(), (int32_t)n, 0, nullptr);
    auto t1=clk::now();
    double d=ms(t0,t1); if(d<t_sais)t_sais=d;
  }
  double t_cm=1e18, t_b9=1e18; size_t s_cm=0,s_b9=0;
  for(int r=0;r<3;r++){
    auto t0=clk::now(); auto cm=cmbk::compress_bwt(b.data(),(size_t)n); auto t1=clk::now();
    double d=ms(t0,t1); if(d<t_cm){t_cm=d;} s_cm=cm.size();
  }
  for(int r=0;r<3;r++){
    auto t0=clk::now(); auto o=bwt9::compress(b.data(),(size_t)n); auto t1=clk::now();
    double d=ms(t0,t1); if(d<t_b9){t_b9=d;} s_b9=o.size();
  }
  double t_legacy = t_b9 - t_cm;              // the bwt5/bwt8 arm
  printf("%-34s n=%-9ld  bwt9=%8.1f ms  [legacy=%7.1f  cmbk=%7.1f]  libsais=%6.1f ms = %4.1f%% of bwt9  (x2 = %4.1f%%)  sizes b9=%zu cm=%zu\n",
         v[1], n, t_b9, t_legacy, t_cm, t_sais, 100.0*t_sais/t_b9, 200.0*t_sais/t_b9, s_b9, s_cm);
  return 0;
}
