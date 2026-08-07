#include <cstdio>
#include <vector>
#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#include "mzip.hpp"
int main(int argc,char**argv){
  const char* fn = argc>1?argv[1]:"fuzzd_cur.bin";
  FILE* f=fopen(fn,"rb"); if(!f){printf("no file\n");return 2;}
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); fread(b.data(),1,n,f); fclose(f);
  printf("decompressing %ld bytes magic=%c%c\n", n, n>0?b[0]:'?', n>1?b[1]:'?'); fflush(stdout);
  auto d = mzip::decompress(b.data(), b.size());
  printf("OK out=%zu\n", d.size());
  return 0;
}
