#include "zstd_release/zstd-v1.5.6-win64/include/zstd.h"
#define MZIP_IMPLEMENTATION
#include "mzip_amalgamated.hpp"
#include <cstdio>
#include <vector>
#include <cstring>
// A gate must FAIL CLOSED. Two ways this one used to pass vacuously: `rt` returned -2 for an
// unreadable file while main only counted `> 0`, so missing files were silently skipped; and with
// zero arguments the loop never ran, so it printed "AMALG OK" and exited 0 having tested nothing.
// build_evals.sh invokes it as `amalg_test $(ls real_bench/* | head -3)`, which expands to NOTHING
// if the corpus is absent -- i.e. exactly the case where the gate must not say OK. (2026-08-12)
static int rt(const char* fn){
  FILE* f=fopen(fn,"rb"); if(!f){ printf("%-40s CANNOT OPEN\n", fn); return 1; }
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n);
  if(n>0 && fread(b.data(),1,n,f)!=(size_t)n){ fclose(f); printf("%-40s SHORT READ\n", fn); return 1; }
  fclose(f);
  auto c = mzip::compress(b.data(), b.size());
  auto d = mzip::decompress(c.data(), c.size());
  bool ok = d.size()==b.size() && (b.empty()||memcmp(d.data(),b.data(),b.size())==0);
  printf("%-40s in=%ld comp=%zu lossless=%s\n", fn, n, c.size(), ok?"YES":"NO"); return ok?0:1;
}
int main(int argc,char**argv){
  if(argc<2){ printf("AMALG FAIL: no input files given (a gate that tests nothing must not pass)\n"); return 2; }
  int bad=0; for(int i=1;i<argc;i++) if(rt(argv[i])!=0) bad++;
  printf(bad?"AMALG FAIL\n":"AMALG OK\n"); return bad?1:0;
}
