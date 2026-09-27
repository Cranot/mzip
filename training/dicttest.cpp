#include <cstdint>
#include "zstd.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <dirent.h>
extern "C" {
  size_t ZDICT_trainFromBuffer(void*,size_t,const void*,const size_t*,unsigned);
  unsigned ZDICT_isError(size_t);
}
static std::vector<uint8_t> readf(const char*p){FILE*f=fopen(p,"rb");if(!f)return{};fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);std::vector<uint8_t>b(n);if((long)fread(b.data(),1,n,f)!=n){};fclose(f);return b;}
int main(int argc,char**argv){
  std::string dir=argv[1]; int kb=atoi(argv[2]);
  std::vector<uint8_t> samples; std::vector<size_t> sizes;
  DIR*d=opendir(dir.c_str()); struct dirent*e;
  while(d&&(e=readdir(d))){ std::string n=e->d_name; if(n=="."||n=="..")continue; auto b=readf((dir+"/"+n).c_str()); if(b.size()){samples.insert(samples.end(),b.begin(),b.end());sizes.push_back(b.size());} }
  if(d)closedir(d);
  std::vector<uint8_t> dict(kb*1024);
  size_t ds=ZDICT_trainFromBuffer(dict.data(),dict.size(),samples.data(),sizes.data(),(unsigned)sizes.size());
  if(ZDICT_isError(ds)){printf("train fail\n");return 1;}
  printf("trained dict: %zu bytes from %zu samples\n",ds,sizes.size());
  for(int i=3;i<argc;i++){
    auto b=readf(argv[i]); if(b.empty())continue;
    ZSTD_CCtx*c=ZSTD_createCCtx(); std::vector<uint8_t>o(ZSTD_compressBound(b.size()));
    size_t s=ZSTD_compress_usingDict(c,o.data(),o.size(),b.data(),b.size(),dict.data(),ds,19);
    ZSTD_freeCCtx(c);
    printf("  %-28s zstd19+%dKBdict=%zu\n",argv[i],kb,s);
  }
  return 0;
}
