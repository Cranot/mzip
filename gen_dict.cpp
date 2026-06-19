// gen_dict.cpp — build a frequency-ranked word dictionary for dict-as-context.
// Replicates lpaq_x.cpp's EXACT word hash (wordh=(wordh+lower(c))*(7<<3); cword=wordh at word end),
// counts cword frequencies over the input, writes the top-N cword values (by frequency) to dict.bin
// as a U32 array (array index = rank 0..N-1). The coder loads this and feeds word-rank as a mixer context.
// Usage: gen_dict <corpus> <dict.bin> [N]
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <unordered_map>
typedef uint32_t U32;
int main(int argc,char**argv){
  if(argc<3){fprintf(stderr,"usage: gen_dict <corpus> <dict.bin> [N=65535]\n");return 1;}
  int N=argc>3?atoi(argv[3]):65535;
  FILE*f=fopen(argv[1],"rb"); if(!f){perror(argv[1]);return 1;}
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  std::vector<unsigned char> buf(sz); if((long)fread(buf.data(),1,sz,f)!=sz){return 1;} fclose(f);
  std::unordered_map<U32,U32> freq; freq.reserve(1<<20);
  U32 wordh=0;
  for(long i=0;i<sz;i++){
    int c=buf[i];
    int low=(c>=97&&c<=122)?c:((c>=65&&c<=90)?c+32:0);   // EXACT match to lpaq_x word stack
    if(low) wordh=(wordh+(U32)low)*(7u<<3);
    else { if(wordh) freq[wordh]++; wordh=0; }
  }
  if(wordh) freq[wordh]++;
  std::vector<std::pair<U32,U32>> v(freq.begin(),freq.end());
  std::sort(v.begin(),v.end(),[](const std::pair<U32,U32>&a,const std::pair<U32,U32>&b){
    return a.second>b.second; });
  int n=(int)v.size(); if(n>N) n=N;
  FILE*o=fopen(argv[2],"wb"); if(!o){perror(argv[2]);return 1;}
  for(int i=0;i<n;i++) fwrite(&v[i].first,4,1,o);
  fclose(o);
  fprintf(stderr,"dict: %d unique words, wrote top %d to %s\n",(int)v.size(),n,argv[2]);
  return 0;
}
