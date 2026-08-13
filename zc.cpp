// zc.cpp -- zstd size tool for benchmarks.
//   zc <file> [level]          -> compressed size (no dict)          [existing interface]
//   zc <file> <level> <dict>   -> compressed size using a zstd dict  [ZSTD_compress_usingDict]
//   zc train <outdict> <dictBytes> <file1> [file2 ...]  -> train a zstd dict (ZDICT), write it, print size
// The dict mode gives a FAIR "zstd with a dictionary" comparator (dict-vs-dict vs mzip/brotli).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "zstd.h"
// ZDICT lives in zdict.h (not bundled here); forward-declare the trainer (symbol is in libzstd_static).
extern "C" {
  size_t ZDICT_trainFromBuffer(void* dictBuffer, size_t dictBufferCapacity,
                               const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples);
  unsigned ZDICT_isError(size_t);
  const char* ZDICT_getErrorName(size_t);
}
static std::vector<char> rd(const char* p){
  FILE* f=fopen(p,"rb"); std::vector<char> b; if(!f) return b;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  if(n>0){ b.resize(n); if((long)fread(b.data(),1,n,f)!=n) b.clear(); } fclose(f); return b;
}
int main(int argc,char**argv){
  // --version: pin WHICH zstd produced the benchmark's zstd column. Without this the zstd rows were
  // the only comparator in the matrix with no version at all, and zstd's level-19/22 output has
  // changed across releases -- so "zstd-19" alone did not identify a measurement.
  if(argc>=2 && (strcmp(argv[1],"--version")==0 || strcmp(argv[1],"-V")==0)){
    printf("zc (zstd sizer) linked against zstd %s\n", ZSTD_versionString()); return 0;
  }
  if(argc>=5 && strcmp(argv[1],"train")==0){
    const char* outdict=argv[2]; size_t cap=(size_t)strtoul(argv[3],0,10);
    std::vector<char> samples; std::vector<size_t> sizes;
    for(int i=4;i<argc;i++){ auto s=rd(argv[i]); if(!s.empty()){ samples.insert(samples.end(),s.begin(),s.end()); sizes.push_back(s.size()); } }
    if(sizes.empty()){ fprintf(stderr,"no samples\n"); return 1; }
    std::vector<char> dict(cap);
    size_t ds=ZDICT_trainFromBuffer(dict.data(),cap,samples.data(),sizes.data(),(unsigned)sizes.size());
    if(ZDICT_isError(ds)){ fprintf(stderr,"train error: %s\n",ZDICT_getErrorName(ds)); return 1; }
    FILE* o=fopen(outdict,"wb"); if(!o){ fprintf(stderr,"cannot write %s\n",outdict); return 1; }
    fwrite(dict.data(),1,ds,o); fclose(o); printf("%zu\n",ds); return 0;
  }
  if(argc<2){ printf("0\n"); return 1; }
  int lvl=argc>2?atoi(argv[2]):19;
  auto b=rd(argv[1]); long n=(long)b.size();
  std::vector<char> o(ZSTD_compressBound(n)+16); size_t s;
  if(argc>3){
    auto dict=rd(argv[3]); ZSTD_CCtx* c=ZSTD_createCCtx();
    s=ZSTD_compress_usingDict(c,o.data(),o.size(),b.data(),n,dict.data(),dict.size(),lvl);
    ZSTD_freeCCtx(c);
  } else {
    s=ZSTD_compress(o.data(),o.size(),b.data(),n,lvl);
  }
  printf("%zu\n", ZSTD_isError(s)?0:s);
  return 0;
}
