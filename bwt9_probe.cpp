#include "bwt_compress_v9.hpp"
#include <cstdio>
#include <vector>
int main(int c,char**v){
  if(c<2){printf("usage: bwt9_probe <file>\n");return 1;}
  FILE*f=fopen(v[1],"rb"); fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n)return 1; fclose(f);
  auto o=bwt9::compress(b.data(),(size_t)n);
  auto bk=bwt9::decompress(o.data(),o.size());
  printf("bwt9: %ld -> %zu  roundtrip=%s\n", n, o.size(), (bk.size()==(size_t)n&&bk==b)?"OK":"FAIL");
  return 0;
}
