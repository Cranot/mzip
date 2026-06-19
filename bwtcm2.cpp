// bwtcm2 — Path A: lpaq-style ICM (bit-history->StateMap) + logistic Mixer + orthogonal APMs
// on raw BWT bytes. Goal: pass bsc-m03 (1.280 bpc). Components ported from lpaq1 (Mahoney).
// build: g++ -O3 -std=c++17 bwtcm2.cpp libsais.o -o bwtcm2.exe
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <chrono>
#include "libsais.h"
using namespace std;
typedef uint8_t U8; typedef uint16_t u16; typedef uint32_t U32; typedef uint64_t u64;

int squash(int d){
    static const int t[33]={1,2,3,6,10,16,27,45,73,120,194,310,488,747,1101,1546,2047,
        2549,2994,3348,3607,3785,3901,3975,4022,4050,4068,4079,4085,4089,4092,4093,4094};
    if(d>2047)return 4095; if(d<-2047)return 0;
    int w=d&127; d=(d>>7)+16; return (t[d]*(128-w)+t[d+1]*w+64)>>7;
}
struct Stretch{ short t[4096];
    Stretch(){ int pi=0; for(int x=-2047;x<=2047;++x){int i=squash(x); for(int j=pi;j<=i;++j)t[j]=(short)x; pi=i+1;} t[4095]=2047; }
    int operator()(int p)const{ return t[p]; }
} stretch;

static const U8 State_table[256][2]={
{1,2},{3,5},{4,6},{7,10},{8,12},{9,13},{11,14},
{15,19},{16,23},{17,24},{18,25},{20,27},{21,28},{22,29},
{26,30},{31,33},{32,35},{32,35},{32,35},{32,35},{34,37},
{34,37},{34,37},{34,37},{34,37},{34,37},{36,39},{36,39},
{36,39},{36,39},{38,40},{41,43},{42,45},{42,45},{44,47},
{44,47},{46,49},{46,49},{48,51},{48,51},{50,52},{53,43},
{54,57},{54,57},{56,59},{56,59},{58,61},{58,61},{60,63},
{60,63},{62,65},{62,65},{50,66},{67,55},{68,57},{68,57},
{70,73},{70,73},{72,75},{72,75},{74,77},{74,77},{76,79},
{76,79},{62,81},{62,81},{64,82},{83,69},{84,71},{84,71},
{86,73},{86,73},{44,59},{44,59},{58,61},{58,61},{60,49},
{60,49},{76,89},{76,89},{78,91},{78,91},{80,92},{93,69},
{94,87},{94,87},{96,45},{96,45},{48,99},{48,99},{88,101},
{88,101},{80,102},{103,69},{104,87},{104,87},{106,57},{106,57},
{62,109},{62,109},{88,111},{88,111},{80,112},{113,85},{114,87},
{114,87},{116,57},{116,57},{62,119},{62,119},{88,121},{88,121},
{90,122},{123,85},{124,97},{124,97},{126,57},{126,57},{62,129},
{62,129},{98,131},{98,131},{90,132},{133,85},{134,97},{134,97},
{136,57},{136,57},{62,139},{62,139},{98,141},{98,141},{90,142},
{143,95},{144,97},{144,97},{68,57},{68,57},{62,81},{62,81},
{98,147},{98,147},{100,148},{149,95},{150,107},{150,107},{108,151},
{108,151},{100,152},{153,95},{154,107},{108,155},{100,156},{157,95},
{158,107},{108,159},{100,160},{161,105},{162,107},{108,163},{110,164},
{165,105},{166,117},{118,167},{110,168},{169,105},{170,117},{118,171},
{110,172},{173,105},{174,117},{118,175},{110,176},{177,105},{178,117},
{118,179},{110,180},{181,115},{182,117},{118,183},{120,184},{185,115},
{186,127},{128,187},{120,188},{189,115},{190,127},{128,191},{120,192},
{193,115},{194,127},{128,195},{120,196},{197,115},{198,127},{128,199},
{120,200},{201,115},{202,127},{128,203},{120,204},{205,115},{206,127},
{128,207},{120,208},{209,125},{210,127},{128,211},{130,212},{213,125},
{214,137},{138,215},{130,216},{217,125},{218,137},{138,219},{130,220},
{221,125},{222,137},{138,223},{130,224},{225,125},{226,137},{138,227},
{130,228},{229,125},{230,137},{138,231},{130,232},{233,125},{234,137},
{138,235},{130,236},{237,125},{238,137},{138,239},{130,240},{241,125},
{242,137},{138,243},{130,244},{245,135},{246,137},{138,247},{140,248},
{249,135},{250,69},{80,251},{140,252},{249,135},{250,69},{80,251},
{140,252},{0,0},{0,0},{0,0}};
#define nex(s,sel) State_table[s][sel]

struct StateMap{
    int N, cxt; U32* t; static int dt[1024];
    StateMap(int n=256):N(n),cxt(0){ t=new U32[N]; for(int i=0;i<N;i++)t[i]=1u<<31;
        if(dt[0]==0) for(int i=0;i<1024;i++) dt[i]=16384/(i+i+3); }
    void update(int y,int limit){ int n=t[cxt]&1023,p=t[cxt]>>10;
        if(n<limit)++t[cxt]; else t[cxt]=(t[cxt]&0xfffffc00)|limit;
        t[cxt]+=((((y<<22)-p)>>3)*dt[n])&0xfffffc00; }
    int p(int y,int cx,int limit=1023){ update(y,limit); return t[cxt=cx]>>20; }
};
int StateMap::dt[1024]={0};

struct APM: StateMap{
    APM(int n):StateMap(n*24){ for(int i=0;i<N;i++){ int p=((i%24*2+1)*4096)/48-2048; t[i]=((U32)squash(p)<<20)+6; } }
    int pp(int y,int pr,int cx,int limit=255){ update(y,limit); pr=(stretch(pr)+2048)*23;
        int wt=pr&0xfff; cx=cx*24+(pr>>12); pr=((t[cx]>>13)*(0x1000-wt)+(t[cx+1]>>13)*wt)>>19; cxt=cx+(wt>>11); return pr; }
};

struct Mixer{
    int N,M,cxt,nx,pr; int* tx; int* wx;
    Mixer(int n,int m):N(n),M(m),cxt(0),nx(0),pr(2048){ tx=new int[N]; wx=new int[N*M](); }
    void add(int x){ tx[nx++]=x; }
    void set(int cx){ cxt=cx; }
    int p(){ int sum=0; int* w=wx+cxt*N; for(int i=0;i<N;i++) sum+=tx[i]*w[i]; pr=squash((sum>>8)>>8); return pr; }
    void update(int y){ int err=((y<<12)-pr)*7; int* w=wx+cxt*N; for(int i=0;i<N;i++) w[i]+=(tx[i]*err+0x8000)>>16; nx=0; }
};

// ---- range coder (bzip3-style carryless, from bwtcm) ----
struct Enc{ U32 low=0,high=0xFFFFFFFFu; vector<U8>& out; Enc(vector<U8>&o):out(o){}
    void bit(int b,int pr){ U32 mid=low+(U32)(((u64)(high-low)*(U32)pr)>>18); if(b)high=mid; else low=mid+1;
        while((low^high)<(1u<<24)){ out.push_back(low>>24); low<<=8; high=(high<<8)|0xFF; } }
    void flush(){ for(int i=0;i<4;i++){ out.push_back(low>>24); low<<=8; } } };
struct Dec{ U32 low=0,high=0xFFFFFFFFu,code=0; const U8* in; size_t pos=0,len; Dec(const U8*i,size_t l):in(i),len(l){ for(int k=0;k<4;k++)code=(code<<8)|rd(); }
    U8 rd(){ return pos<len?in[pos++]:0; }
    int bit(int pr){ U32 mid=low+(U32)(((u64)(high-low)*(U32)pr)>>18); int b=(code<=mid)?1:0; if(b)high=mid; else low=mid+1;
        while((low^high)<(1u<<24)){ code=(code<<8)|rd(); low<<=8; high=(high<<8)|0xFF; } return b; } };

// ---- ICM model over BWT bytes (orders 0..4 + run) ----
static const int HBITS=22;
static inline U32 hmix(U32 h){ h^=h>>15; h*=2246822519u; h^=h>>13; h*=3266489917u; h^=h>>16; return h&((1u<<HBITS)-1); }

struct Model {
    vector<U8> t0,t1,t2,t3,t4;
    StateMap sm0,sm1,sm2,sm3,sm4;
    Mixer mix; APM apm1,apm2;
    int c1=0,c2=0,c3=0,c4=0,run=0,y=0,ctx=1,rb=0;
    int i0,i1; U32 i2,i3,i4;
    Model(): t0(256,0),t1(65536,0),t2((size_t)1<<HBITS,0),t3((size_t)1<<HBITS,0),t4((size_t)1<<HBITS,0),
             sm0(256),sm1(256),sm2(256),sm3(256),sm4(256), mix(5,256), apm1(256),apm2(8) {}
    void byte_start(){ rb=run>7?7:run; ctx=1; }
    int predict(){
        i0=ctx; i1=(c1<<8)|ctx;
        i2=hmix((U32)c1*2654435761u + (U32)c2*2246822519u + (U32)ctx*0x9E3779B1u + 1);
        i3=hmix((U32)c1*2654435761u + (U32)c2*2246822519u + (U32)c3*3266489917u + (U32)ctx*0x9E3779B1u + 2);
        i4=hmix((U32)c1*2654435761u + (U32)c2*2246822519u + (U32)c3*3266489917u + (U32)c4*668265263u + (U32)ctx*0x9E3779B1u + 3);
        mix.nx=0;
        mix.add(stretch(sm0.p(y,t0[i0])));
        mix.add(stretch(sm1.p(y,t1[i1])));
        mix.add(stretch(sm2.p(y,t2[i2])));
        mix.add(stretch(sm3.p(y,t3[i3])));
        mix.add(stretch(sm4.p(y,t4[i4])));
        mix.set(ctx);
        int pr=mix.p();
        pr=(pr+3*apm1.pp(y,pr,ctx))>>2;
        pr=(pr+3*apm2.pp(y,pr,rb))>>2;
        int pr18=pr<<6; if(pr18<1)pr18=1; else if(pr18>262143)pr18=262143;
        return pr18;
    }
    void update_bit(int bit){
        t0[i0]=nex(t0[i0],bit); t1[i1]=nex(t1[i1],bit);
        t2[i2]=nex(t2[i2],bit); t3[i3]=nex(t3[i3],bit); t4[i4]=nex(t4[i4],bit);
        mix.update(bit); y=bit; ctx=ctx*2+bit;
    }
    void byte_end(int c){ run=(c==c1)?run+1:0; c4=c3;c3=c2;c2=c1;c1=c; }
};

int main(int argc,char**argv){
    if(argc<3){ printf("usage: bwtcm2 <file> <maxbytes>\n"); return 1; }
    long long mx=atoll(argv[2]);
    FILE*fp=fopen(argv[1],"rb"); fseek(fp,0,SEEK_END); long long fsz=ftell(fp); fseek(fp,0,SEEK_SET);
    if(mx>0&&mx<fsz) fsz=mx; int n=(int)fsz; vector<U8> T(n); fread(T.data(),1,n,fp); fclose(fp);
    vector<int32_t> A(n+1);
    auto t0=chrono::steady_clock::now();
    libsais_bwt(T.data(),T.data(),A.data(),n,0,nullptr);
    auto t1=chrono::steady_clock::now();
    vector<U8> comp; comp.reserve(n/3);
    { Enc e(comp); Model m; for(int p=0;p<n;p++){ m.byte_start(); int c=T[p];
        for(int bi=0;bi<8;bi++){ int pr=m.predict(); int bit=(c>>(7-bi))&1; e.bit(bit,pr); m.update_bit(bit); } m.byte_end(c); } e.flush(); }
    auto t2=chrono::steady_clock::now();
    vector<U8> dec(n);
    { Dec d(comp.data(),comp.size()); Model m; for(int p=0;p<n;p++){ m.byte_start();
        for(int bi=0;bi<8;bi++){ int pr=m.predict(); int bit=d.bit(pr); m.update_bit(bit); } int c=m.ctx&255; dec[p]=(U8)c; m.byte_end(c); } }
    bool ok=(dec==T);
    double bwt_ms=chrono::duration<double,milli>(t1-t0).count();
    double enc_ms=chrono::duration<double,milli>(t2-t1).count();
    long long total=(long long)comp.size()+8;
    printf("ICM5 n=%d BWT=%.0fms enc=%.0fms comp=%lld %.4f bpc ok=%d\n",n,bwt_ms,enc_ms,total,total*8.0/n,ok);
    return 0;
}
