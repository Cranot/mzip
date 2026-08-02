// Which bwt5 candidate actually wins? Each variant costs a compress_huffman
// (~26 ms on 287 KB, the dominant per-candidate cost), so a variant that never
// wins is pure spend. Header byte out[1] identifies the winner.
#include "bwt_compress_v5.hpp"
#include <cstdio>
#include <vector>
int main(int c,char**v){
  if(c<2)return 1;
  FILE*f=fopen(v[1],"rb"); if(!f)return 1;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  if(n<=0){fclose(f);return 1;}
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n){fclose(f);return 1;} fclose(f);
  auto o = bwt5::compress(b.data(),(size_t)n);
  if(o.size()<2){ printf("%s\t%ld\t?\t0\n",v[1],n); return 0; }
  printf("%s\t%ld\t%c\t%zu\n", v[1], n, o[1], o.size());
  return 0;
}
