// Would a provable lower bound prune the expensive Huffman candidates?
// Multi-tree Huffman gives every symbol >=1 bit, so any candidate is
// >= ceil(nsym/8) bytes. If that already exceeds the incumbent size, the
// candidate cannot win and skipping it leaves min(candidates) unchanged.
// NOTE the bound is valid ONLY for prefix codes -- the range-coder arms (E/F/I/J)
// can go below 1 bit/symbol, so they are excluded by construction.
#include "bwt_compress_v5.hpp"
#include <cstdio>
#include <vector>
int main(int c,char**v){
  if(c<3){printf("usage: bwt5_bound_probe <file> <incumbent_bytes>\n");return 1;}
  size_t cap = (size_t)atoll(v[2]);
  FILE*f=fopen(v[1],"rb"); if(!f)return 1;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<uint8_t> b(n); if((long)fread(b.data(),1,n,f)!=n){fclose(f);return 1;} fclose(f);

  auto rle1  = bwt5::pre_rle_encode(b.data(),(size_t)n);
  auto pw    = bwt5::run_bwt_pipeline(rle1.data(), rle1.size());
  auto pwo   = bwt5::run_bwt_pipeline(b.data(), (size_t)n);

  printf("%s  n=%ld  incumbent cap=%zu\n", v[1], n, cap);
  if(pw.ok){
    size_t lb=(pw.rle.size()+7)/8;
    printf("  pipeline WITH pre-RLE : nsym=%9zu  lower bound=%9zu B  -> %s\n",
           pw.rle.size(), lb, lb>=cap ? "PRUNES (provably cannot win)" : "does not prune");
  }
  if(pwo.ok){
    size_t lb=(pwo.rle.size()+7)/8;
    printf("  pipeline NO  pre-RLE  : nsym=%9zu  lower bound=%9zu B  -> %s\n",
           pwo.rle.size(), lb, lb>=cap ? "PRUNES (provably cannot win)" : "does not prune");
  }
  return 0;
}
