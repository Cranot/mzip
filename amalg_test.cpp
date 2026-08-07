#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#define MZIP_IMPLEMENTATION
#include "mzip_amalgamated.hpp"
#include <cstdio>
#include <vector>
#include <cstring>
static int rt(const char* fn){
  FILE* f=fopen(fn,"rb"); if(!f) return -2; fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); fread(b.data(),1,n,f); fclose(f);
  auto c = mzip::compress(b.data(), b.size());
  auto d = mzip::decompress(c.data(), c.size());
  bool ok = d.size()==b.size() && (b.empty()||memcmp(d.data(),b.data(),b.size())==0);
  printf("%-40s in=%ld comp=%zu lossless=%s\n", fn, n, c.size(), ok?"YES":"NO"); return ok?0:1;
}
int main(int argc,char**argv){ int bad=0; for(int i=1;i<argc;i++) if(rt(argv[i])>0) bad++; printf(bad?"AMALG FAIL\n":"AMALG OK\n"); return bad?1:0; }
