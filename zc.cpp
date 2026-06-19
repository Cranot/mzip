#include <cstdio>
#include <cstdlib>
#include <vector>
#include "zstd.h"
int main(int argc,char**argv){
  if(argc<2){printf("0\n");return 1;}
  int lvl=argc>2?atoi(argv[2]):19;
  FILE*f=fopen(argv[1],"rb"); if(!f){printf("0\n");return 1;}
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<char> b(n>0?n:1); if(n>0 && (long)fread(b.data(),1,n,f)!=n){fclose(f);printf("0\n");return 1;} fclose(f);
  std::vector<char> o(ZSTD_compressBound(n)+16);
  size_t s=ZSTD_compress(o.data(),o.size(),b.data(),n,lvl);
  printf("%zu\n", ZSTD_isError(s)?0:s);
  return 0;
}
