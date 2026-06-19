#include "brotli_shim.hpp"
#include "lzma_optimal2.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int c,char**v){
  if(c<2)return 1;
  FILE*f=fopen(v[1],"rb"); fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n)return 1; fclose(f);
  for(int m=0;m<2;m++){ size_t cap=BrotliEncoderMaxCompressedSize(n); std::vector<uint8_t> bb(cap); size_t s=cap;
    BrotliEncoderCompress(11,24,m,(size_t)n,b.data(),&s,bb.data()); printf("brotli-lib mode%d=%zu  ",m,s); }
  uint32_t d=1u<<22; while(d<(uint32_t)n&&d<(1u<<26))d<<=1;
  auto lz=lzma_opt2::compress(b.data(),(size_t)n,d,3); printf("lzma-L3=%zu  orig=%ld\n",lz.size(),n);
  return 0;
}
