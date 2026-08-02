// bwt_compress_v5.hpp:1390 claims "The BWT runs dominate cost; the entropy
// backends are cheap by comparison." Testing that claim against the clock.
#include "bwt_compress_v5.hpp"
#include <cstdio>
#include <vector>
#include <chrono>
using clk = std::chrono::steady_clock;
static double ms(clk::time_point a, clk::time_point b){ return std::chrono::duration<double,std::milli>(b-a).count(); }
template<class F> static double best(F f,int r=3){ double m=1e18; for(int i=0;i<r;i++){auto t0=clk::now(); f(); auto t1=clk::now(); double d=ms(t0,t1); if(d<m)m=d;} return m; }
int main(int c,char**v){
  if(c<2)return 1;
  FILE*f=fopen(v[1],"rb"); if(!f)return 1;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n){fclose(f);return 1;} fclose(f);

  double t_total = best([&]{ bwt5::compress(b.data(),(size_t)n); },2);
  auto out = bwt5::compress(b.data(),(size_t)n);
  double t_rt = best([&]{ bwt5::decompress(out.data(),out.size(),false); });

  auto pipe = bwt5::run_bwt_pipeline(b.data(),(size_t)n);
  double t_pipe_raw = best([&]{ bwt5::run_bwt_pipeline(b.data(),(size_t)n); });
  double t_huff=0, t_rc0=0, t_rc1=0;
  if(pipe.ok){
    t_huff = best([&]{ bwt5::compress_huffman(pipe,(uint32_t)n,'D'); });
    t_rc0  = best([&]{ bwt5::compress_rc(pipe,(uint32_t)n,'F'); });
    t_rc1  = best([&]{ bwt5::compress_rc(pipe,(uint32_t)n,'J'); });
  }
  printf("%-32s n=%-8ld total=%7.1f | pipeline(raw)=%6.1f  huffman=%6.1f  rc_o0=%6.1f  rc_o1=%6.1f  roundtrip_verify=%6.1f (%4.1f%%)\n",
         v[1], n, t_total, t_pipe_raw, t_huff, t_rc0, t_rc1, t_rt, 100.0*t_rt/t_total);
  return 0;
}
