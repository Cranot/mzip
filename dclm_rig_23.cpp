/* dclm_rig.cpp — Dynamical Context Lattice Mixer: log-loss filter rig.
 *
 * A standalone bitwise context-mixing log-loss evaluator (NO arithmetic coding;
 * it measures the coded cost directly as sum -log2(p_actual)). Machinery
 * (squash/stretch/StateMap/Mixer) is lifted verbatim from lpaq1 (Mahoney, GPL),
 * the same fork this project already ships as ref_lpaq1.cpp.
 *
 * Purpose: cheaply test whether deterministic integer DYNAMICAL state
 * (oscillator reservoir / input-switched affine / hypervector pad) or
 * cross-product projections carry predictive signal ABOVE a mid-strength
 * static-context baseline — and crucially, whether any apparent win survives
 * the controls (frozen / shuffled / random-key) rather than being mere extra
 * table capacity.
 *
 * Usage:  dclm_rig <file> <module> <ctrl>
 *   module: base | xprod | osc | aff | hv
 *   ctrl  : real | frozen | shuffled | random      (baseline is always real)
 * Output (stderr): one line "module ctrl file  bytes  bpc  models"
 *
 * Build: g++ -O3 -std=c++17 dclm_rig.cpp -o dclm_rig.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <vector>
#include <string>

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

static void quit(const char* m){ if(m) fprintf(stderr,"%s\n",m); exit(1); }
template<class T> void alloc(T*&p,size_t n){ p=(T*)calloc(n,sizeof(T)); if(!p) quit("oom"); }

/////////////////// squash / stretch (lpaq verbatim) ///////////////////
int squash(int d){
  static const int t[33]={1,2,3,6,10,16,27,45,73,120,194,310,488,747,1101,
    1546,2047,2549,2994,3348,3607,3785,3901,3975,4022,4050,4068,4079,4085,4089,4092,4093,4094};
  if(d>2047) return 4095;
  if(d<-2047) return 0;
  int w=d&127; d=(d>>7)+16;
  return (t[d]*(128-w)+t[(d+1)]*w+64)>>7;
}
class Stretch{ short t[4096]; public:
  Stretch(){ int pi=0; for(int x=-2047;x<=2047;++x){ int i=squash(x); for(int j=pi;j<=i;++j) t[j]=x; pi=i+1; } t[4095]=2047; }
  int operator()(int p) const { return t[p]; }
} stretch;

/////////////////// StateMap (lpaq, keyed by hashed context) ///////////////////
class StateMap{
protected:
  const int N; int cxt; U32 *t; static int dt[1024];
  void update(int y,int limit){
    int n=t[cxt]&1023, p=t[cxt]>>10;
    if(n<limit) ++t[cxt]; else t[cxt]=(t[cxt]&0xfffffc00)|limit;
    t[cxt]+=((((y<<22)-p)>>3)*dt[n])&0xfffffc00;
  }
public:
  StateMap(int n=256):N(n),cxt(0){ alloc(t,N); for(int i=0;i<N;++i) t[i]=1u<<31;
    if(dt[0]==0) for(int i=0;i<1024;++i) dt[i]=16384/(i+i+3); }
  int p(int y,int cx,int limit=1023){ update(y,limit); return t[cxt=cx]>>20; }
};
int StateMap::dt[1024]={0};

/////////////////// Mixer (lpaq verbatim) ///////////////////
inline void train(int*t,int*w,int n,int err){ for(int i=0;i<n;++i) w[i]+=(t[i]*err+0x8000)>>16; }
inline int dot(int*t,int*w,int n){ int s=0; for(int i=0;i<n;++i) s+=t[i]*w[i]; return s>>8; }
class Mixer{
  const int N,M; int *tx,*wx,cxt,nx,pr;
public:
  Mixer(int n,int m):N(n),M(m),cxt(0),nx(0),pr(2048){ alloc(tx,N); alloc(wx,N*M);
    for(int i=0;i<N*M;++i) wx[i]=0; }
  void upd(int y){ int err=((y<<12)-pr)*7; train(&tx[0],&wx[cxt*N],N,err); nx=0; }
  void add(int x){ tx[nx++]=x; }
  void set(int cx){ cxt=cx; }
  int p(){ return pr=squash(dot(&tx[0],&wx[cxt*N],N)>>8); }
};

/////////////////// hashing ///////////////////
static inline U32 fmix(U32 h){ h^=h>>16; h*=0x7feb352d; h^=h>>15; h*=0x846ca68b; h^=h>>16; return h; }
static inline U32 H(U32 a,U32 b=0,U32 c=0,U32 d=0,U32 e=0,U32 f=0){
  U32 h=2166136261u;
  h=(h^a)*16777619u; h=(h^b)*16777619u; h=(h^c)*16777619u;
  h=(h^d)*16777619u; h=(h^e)*16777619u; h=(h^f)*16777619u;
  return fmix(h);
}

/////////////////// control LCG ///////////////////
enum { C_REAL, C_FROZEN, C_SHUFFLED, C_RANDOM };
static int CTRL=C_REAL;
static U64 g_lcg=0x243F6A8885A308D3ULL;
static inline U32 lcg32(){ g_lcg=g_lcg*6364136223846793005ULL+1442695040888963407ULL; return (U32)(g_lcg>>33); }

/////////////////// char class ///////////////////
static inline int cclass(int c){
  if(c=='\n') return 6;
  if(c==' '||c=='\t'||c=='\r') return 0;
  if(c>=97&&c<=122) return 1;     // lower
  if(c>=65&&c<=90)  return 2;     // upper
  if(c>=48&&c<=57)  return 3;     // digit
  if(c>=128)        return 5;     // high byte
  return 4;                       // punctuation / other ascii
}

/////////////////// configuration ///////////////////
enum { M_BASE, M_XPROD, M_OSC, M_OSC2, M_AFF, M_HV, M_XP1, M_XP2, M_XP3, M_XP4 };
static int MODULE=M_BASE;
static const int LOGTAB=23;           // StateMap size per model = 2^21 (8 MB)
static const U32 MASK=(1u<<LOGTAB)-1;

/////////////////// baseline + module state (per completed byte) ///////////////////
static U32 c1=0,c2=0,c3=0,c4=0;        // last 4 whole bytes
static U32 h6=0;                       // order-6 rolling hash
static U32 wordh=0,cword=0;            // current/last word hash
static U32 grp=0;                      // char-class skeleton (rolling)
static U32 caseh=0;                    // case bit-history
static int col=0;                      // column since newline
static int wlen=0;                     // current word length
static int xml=0;                      // wiki structural mode
static int pc=0;                       // previous byte (for digraphs)

// --- oscillator reservoir ---
static const int OSC_N=8;
static U32 phase[OSC_N]; static int32_t vel[OSC_N];
static const int damp[OSC_N]={2,3,4,5,6,8,10,12};
// --- input-switched affine ---
static int16_t sA[16];
// --- hypervector pad ---
static int8_t hv[32];

static void reset_state(){
  c1=c2=c3=c4=h6=wordh=cword=grp=caseh=0; col=wlen=xml=pc=0;
  for(int i=0;i<OSC_N;++i){ phase[i]=0; vel[i]=0; }
  for(int i=0;i<16;++i) sA[i]=0;
  for(int i=0;i<32;++i) hv[i]=0;
  g_lcg=0x243F6A8885A308D3ULL;
}

// update wiki structural mode given completed byte cc and prev pc
static inline void upd_xml(int cc){
  // digraph-driven approximate state machine over enwik markup
  if(cc=='<') { xml=1; }                                   // enter tag
  else if(xml==1 && cc=='>') xml=0;                        // leave tag
  else if(pc=='[' && cc=='[') xml=2;                       // enter [[link]]
  else if(xml==2 && pc==']' && cc==']') xml=0;
  else if(pc=='{' && cc=='{') xml=3;                       // enter {{template}}
  else if(xml==3 && pc=='}' && cc=='}') xml=0;
  else if(cc=='\n') { if(xml==4) xml=0; }                  // heading ends at EOL
  else if(col<=1 && cc=='=') xml=4;                        // line starts with '=' -> heading
}

// per-completed-byte state advance.  cc = real byte value.
static void advance_byte(int cc){
  // ---- baseline static state (always real) ----
  c4=(c4<<8)|(U32)cc; c3=c4&0xffffff; c2=c4&0xffff; c1=c4&0xff;
  h6=(h6*(11<<5)+(U32)cc*13)&0x3fffffff;
  int low=(cc>=97&&cc<=122)?cc:((cc>=65&&cc<=90)?cc+32:0);
  if(low){ wordh=(wordh+(U32)low)*(7<<3); wlen++; }
  else { if(wordh){ cword=wordh; } wordh=0; wlen=0; }
  { int cls=(cc>='0'&&cc<='9')?'0':(cc>='A'&&cc<='Z')?'A':(cc>='a'&&cc<='z')?'a':(cc>=128)?128:cc;
    if((int)(grp&0xff)!=cls) grp=(grp<<8)|(U32)cls; }
  { int wu=(cc>=65&&cc<=90); caseh=(caseh<<1)|(U32)wu; }
  if(cc=='\n') col=0; else col++;
  upd_xml(cc);

  // ---- module dynamical state ----
  // Inputs the modules see.  For SHUFFLED control they are replaced by a
  // content-independent LCG stream (same dynamics, zero text coupling).
  U32 mcc, mcls, mxml, mwl;
  if(CTRL==C_SHUFFLED){
    U32 r=lcg32(); mcc=r&0xff; mcls=(U32)cclass((int)mcc); mxml=(r>>8)%5; mwl=(r>>11)&7;
  } else {
    mcc=(U32)cc; mcls=(U32)cclass(cc); mxml=(U32)xml; mwl=(U32)(wlen<7?wlen:7);
  }

  if(MODULE==M_OSC && CTRL!=C_FROZEN){
    for(int i=0;i<OSC_N;++i){
      U32 in=H(0xA0+i, mcc, mcls, mxml, mwl);
      int32_t force=(int32_t)(in&0x7ff)-1024;       // -1024..1023
      vel[i]+=force;
      vel[i]-=vel[i]>>damp[i];
      phase[i]+=(U32)vel[i];
    }
  }
  else if(MODULE==M_OSC2 && CTRL!=C_FROZEN){
    // Fair reservoir: force is a fixed LINEAR projection of ORDERED input
    // features (not a hash), so similar inputs -> similar drive -> smooth
    // text-correlated memory. Win[i][k] is a fixed small signed matrix.
    int feats[4];
    feats[0]=(int)mcc-128;                               // byte centered (ordinal)
    feats[1]=((int)mcls==1?64:((int)mcls==0?-64:0));     // letter(+)/space(-) contrast
    feats[2]=(int)mcls*32-96;                            // class ordinal
    feats[3]=(int)mwl*16-56;                             // word-length ordinal
    for(int i=0;i<OSC_N;++i){
      int32_t force=0;
      for(int k=0;k<4;++k){ int w=(int)(H(0xE0+i,(U32)k)%7)-3; force+=w*feats[k]; }
      vel[i]+=force;
      vel[i]-=vel[i]>>damp[i];
      phase[i]+=(U32)vel[i];
    }
  }
  else if(MODULE==M_AFF && CTRL!=C_FROZEN){
    int cls=(int)(mcls&7);
    int bucket=(int)((mcc>>5)&7);
    int16_t ns[16];
    for(int j=0;j<16;++j){
      U32 g=H(0xB0+cls, j);                          // deterministic per (class,j)
      int perm=(int)(g&15);
      int sign=(g&16)?1:-1;
      int shift=1+(int)((g>>5)&3);                    // 1..4
      int bias=(int)((H(0xC0+cls, j, (U32)bucket)&63))-32; // -32..31
      int32_t v=(int32_t)sA[perm] + sign*((int32_t)sA[j]>>shift) + bias;
      if(v>32767) v=32767; if(v<-32768) v=-32768;
      ns[j]=(int16_t)v;
    }
    memcpy(sA,ns,sizeof(sA));
  }
  else if(MODULE==M_HV && CTRL!=C_FROZEN){
    int rot=(int)(mcc&7)+1;
    int8_t tmp[32];
    for(int j=0;j<32;++j) tmp[j]=hv[(j+rot)&31];     // permutation (lane rotate)
    for(int j=0;j<32;++j){
      int vb=(int)(H(0xD0, mcc, (U32)j)%7)-3;          // symbol vector lane
      int vc=(int)(H(0xD1, mcls,(U32)j)%7)-3;          // class vector lane
      int v=(int)tmp[j]+vb+vc;
      if(v>31) v=31; if(v<-31) v=-31;
      hv[j]=(int8_t)v;
    }
  }
  pc=cc;
}

/////////////////// model index computation per bit ///////////////////
// fills idx[] for the active configuration, returns count. nbase set via *pnbase.
static int build_indices(U32 *idx, int c0, int *pnbase){
  int n=0;
  // ---- baseline (always real) : 9 models ----
  idx[n++]=H(1, c1, (U32)c0)&MASK;                         // order-1
  idx[n++]=H(2, c2, (U32)c0)&MASK;                         // order-2
  idx[n++]=H(3, c3, (U32)c0)&MASK;                         // order-3
  idx[n++]=H(4, c4, (U32)c0)&MASK;                         // order-4
  idx[n++]=H(5, h6, (U32)c0)&MASK;                         // order-6
  idx[n++]=H(6, wordh,(U32)c0)&MASK;                       // current word
  idx[n++]=H(7, grp, (U32)c0)&MASK;                        // char-class skeleton
  idx[n++]=H(8, (U32)(col<255?col:255),(U32)c0)&MASK;      // line/column
  idx[n++]=H(9, (U32)xml, c1,(U32)c0)&MASK;                // wiki mode axis
  *pnbase=n;
  // ---- module ----
  int base=n;
  if(MODULE==M_XPROD){
    idx[n++]=H(20, wordh, grp, (U32)c0)&MASK;              // word x class
    idx[n++]=H(21, caseh&0xff, wordh,(U32)c0)&MASK;        // case x word
    idx[n++]=H(22, (U32)xml, wordh,(U32)c0)&MASK;          // wikimode x word
    idx[n++]=H(23, (U32)xml,(U32)(col<255?col:255),(U32)c0)&MASK; // wikimode x col
  }
  else if(MODULE==M_XP1){ idx[n++]=H(20, wordh, grp,(U32)c0)&MASK; }            // word x class
  else if(MODULE==M_XP2){ idx[n++]=H(21, caseh&0xff, wordh,(U32)c0)&MASK; }     // case x word
  else if(MODULE==M_XP3){ idx[n++]=H(22, (U32)xml, wordh,(U32)c0)&MASK; }       // wikimode x word
  else if(MODULE==M_XP4){ idx[n++]=H(23, (U32)xml,(U32)(col<255?col:255),(U32)c0)&MASK; } // wikimode x col
  else if(MODULE==M_OSC){
    idx[n++]=H(30, phase[0]>>24,(U32)c0)&MASK;             // short-memory phase
    idx[n++]=H(31, phase[2]>>22,(U32)c0)&MASK;             // medium
    idx[n++]=H(32, phase[5]>>20,(U32)xml,(U32)c0)&MASK;    // long x mode
    idx[n++]=H(33,(U32)(vel[1]>>24),(U32)cclass((int)c1),(U32)c0)&MASK;
    idx[n++]=H(34, phase[3]>>23,(U32)(vel[3]>>23),(U32)c0)&MASK;
    idx[n++]=H(35, phase[7]>>22, c1,(U32)c0)&MASK;
  }
  else if(MODULE==M_OSC2){
    idx[n++]=H(30, phase[0]>>26,(U32)c0)&MASK;             // finer/coarser readouts
    idx[n++]=H(31, phase[2]>>24,(U32)c0)&MASK;
    idx[n++]=H(32, phase[4]>>22,(U32)xml,(U32)c0)&MASK;
    idx[n++]=H(33, phase[6]>>20,(U32)c0)&MASK;
    idx[n++]=H(34,(U32)(vel[1]>>22),(U32)cclass((int)c1),(U32)c0)&MASK;
    idx[n++]=H(35, phase[3]>>23,(U32)(vel[5]>>23),(U32)c0)&MASK;
  }
  else if(MODULE==M_AFF){
    idx[n++]=H(40,(U32)(sA[0]>>6),(U32)(sA[1]>>6),(U32)c0)&MASK;
    idx[n++]=H(41,(U32)(sA[2]>>6),(U32)xml,(U32)c0)&MASK;
    idx[n++]=H(42,(U32)(sA[3]>>6),(U32)cclass((int)c1),(U32)c0)&MASK;
    idx[n++]=H(43,(U32)(sA[4]>>6),(U32)(sA[5]>>6),(U32)c0)&MASK;
  }
  else if(MODULE==M_HV){
    U32 s0=0,s1=0;
    for(int j=0;j<16;++j) s0|=((hv[j]<0)?1u:0u)<<j;
    for(int j=0;j<16;++j) s1|=((hv[16+j]<0)?1u:0u)<<j;
    idx[n++]=H(50, s0,(U32)c0)&MASK;
    idx[n++]=H(51, s1,(U32)c0)&MASK;
    idx[n++]=H(52,(U32)(U8)(hv[0]>>2),(U32)(U8)(hv[1]>>2),(U32)(U8)(hv[2]>>2),(U32)c0)&MASK;
    idx[n++]=H(53,(U32)(U8)(hv[8]>>2),(U32)cclass((int)c1),(U32)c0)&MASK;
  }
  // RANDOM control: replace MODULE keys with pure PRNG (same count/table) — capacity control
  if(CTRL==C_RANDOM) for(int i=base;i<n;++i) idx[i]=lcg32()&MASK;
  return n;
}

int main(int argc,char**argv){
  if(argc!=4){ fprintf(stderr,"usage: dclm_rig <file> <base|xprod|osc|aff|hv> <real|frozen|shuffled|random>\n"); return 1; }
  const char* fn=argv[2];
  if(!strcmp(fn,"base"))MODULE=M_BASE; else if(!strcmp(fn,"xprod"))MODULE=M_XPROD;
  else if(!strcmp(fn,"osc"))MODULE=M_OSC; else if(!strcmp(fn,"osc2"))MODULE=M_OSC2;
  else if(!strcmp(fn,"aff"))MODULE=M_AFF;
  else if(!strcmp(fn,"hv"))MODULE=M_HV;
  else if(!strcmp(fn,"xp1"))MODULE=M_XP1; else if(!strcmp(fn,"xp2"))MODULE=M_XP2;
  else if(!strcmp(fn,"xp3"))MODULE=M_XP3; else if(!strcmp(fn,"xp4"))MODULE=M_XP4;
  else quit("bad module");
  const char* cs=argv[3];
  if(!strcmp(cs,"real"))CTRL=C_REAL; else if(!strcmp(cs,"frozen"))CTRL=C_FROZEN;
  else if(!strcmp(cs,"shuffled"))CTRL=C_SHUFFLED; else if(!strcmp(cs,"random"))CTRL=C_RANDOM;
  else quit("bad ctrl");

  FILE* f=fopen(argv[1],"rb"); if(!f) quit("cannot open input");
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  U8* buf=(U8*)malloc(sz); if(!buf) quit("oom buf");
  if((long)fread(buf,1,sz,f)!=sz) quit("read"); fclose(f);

  reset_state();
  // determine model count for this config
  U32 idx[64]; int nbase=0; int N=build_indices(idx,1,&nbase);
  std::vector<StateMap> sm; sm.reserve(N);
  for(int i=0;i<N;++i) sm.emplace_back(1<<LOGTAB);
  Mixer m(N,512);

  int c0=1, bcount=0, ylast=0;
  double loss=0.0;

  for(long bp=0; bp<sz; ++bp){
    int byte=buf[bp];
    for(int b=7;b>=0;--b){
      int bit=(byte>>b)&1;
      // build contexts for the bit we are about to predict
      N=build_indices(idx,c0,&nbase);
      for(int i=0;i<N;++i) m.add(stretch(sm[i].p(ylast, (int)idx[i])));
      int cl=cclass((int)c1);
      m.set( (bcount) | (cl<<3) | ((int)((c1>>5)&7)<<6) );
      int pr=m.p();
      // accumulate coded cost
      double p1=(pr+0.5)/4096.0;
      loss += bit ? -log2(p1) : -log2(1.0-p1);
      // train on the observed bit
      m.upd(bit);
      ylast=bit;
      c0=c0+c0+bit; bcount++;
      if(c0>=256){ c0-=256; advance_byte(c0); c0=1; bcount=0; }
    }
  }
  double bpc=loss/(double)sz;
  long est_bytes=(long)(bpc/8.0*sz);   // = loss/8, the coded size in bytes
  fprintf(stderr,"%-6s %-9s %-16s bytes=%-10ld bpc=%.5f models=%d\n",
    argv[2],argv[3],argv[1],est_bytes,bpc,N);
  // also print machine-readable line to stdout
  printf("%s,%s,%s,%ld,%.6f,%d\n",argv[2],argv[3],argv[1],est_bytes,bpc,N);
  return 0;
}
