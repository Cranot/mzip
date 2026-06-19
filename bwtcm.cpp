// bwtcm — standalone: libsais BWT + bzip3-style context-mixing arithmetic coder.
// Validates our Phase-A coder on REAL enwik at scale. Lossless round-trip verified.
// build: gcc -O3 -c libsais.c -o libsais.o && g++ -O3 -std=c++17 bwtcm.cpp libsais.o -o bwtcm
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <chrono>
#include "libsais.h"
using namespace std;
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;

// ---- bzip3 model state ----
struct Model {
    u16 C0[256]; u16 C1[256][256]; u16 C2[512][17];
    int c1=0, c2=0, run=0;
    Model(){
        for(int i=0;i<256;i++){ C0[i]=32768; for(int j=0;j<256;j++) C1[i][j]=32768; }
        for(int idx=0;idx<512;idx++) for(int k=0;k<17;k++) C2[idx][k]=(u16)((k<<12)-(k==16?1:0));
    }
};
static inline int predict(Model&m,int ctx,int f,int&idx,int&j){
    int p0=m.C0[ctx], p1=m.C1[m.c1][ctx], p2=m.C1[m.c2][ctx];
    int p=((p0+p1)*7 + 2*p2)>>4;
    j=p>>12; idx=2*ctx+f;
    int x1=m.C2[idx][j], x2=m.C2[idx][j+1];
    int ssep=x1 + (((x2-x1)*(p&4095))>>12);
    int pr=ssep*3+p; if(pr<1)pr=1; else if(pr>262143)pr=262143;
    return pr;
}
static inline void upd(Model&m,int ctx,int idx,int j,int bit){
    if(bit){ m.C0[ctx]+=(65535-m.C0[ctx])>>2; m.C1[m.c1][ctx]+=(65535-m.C1[m.c1][ctx])>>4;
             m.C2[idx][j]+=(65535-m.C2[idx][j])>>6; m.C2[idx][j+1]+=(65535-m.C2[idx][j+1])>>6; }
    else   { m.C0[ctx]-=m.C0[ctx]>>2; m.C1[m.c1][ctx]-=m.C1[m.c1][ctx]>>4;
             m.C2[idx][j]-=m.C2[idx][j]>>6; m.C2[idx][j+1]-=m.C2[idx][j+1]>>6; }
}

// ---- carryless binary range coder (bzip3-style) ----
struct Enc {
    u32 low=0, high=0xFFFFFFFFu; vector<u8>& out; Enc(vector<u8>&o):out(o){}
    void bit(int b,int pr){
        u32 mid=low+(u32)(((u64)(high-low)*(u32)pr)>>18);
        if(b) high=mid; else low=mid+1;
        while((low^high)<(1u<<24)){ out.push_back(low>>24); low<<=8; high=(high<<8)|0xFF; }
    }
    void flush(){ for(int i=0;i<4;i++){ out.push_back(low>>24); low<<=8; } }
};
struct Dec {
    u32 low=0, high=0xFFFFFFFFu, code=0; const u8* in; size_t pos=0,len;
    Dec(const u8*i,size_t l):in(i),len(l){ for(int k=0;k<4;k++) code=(code<<8)|rd(); }
    u8 rd(){ return pos<len?in[pos++]:0; }
    int bit(int pr){
        u32 mid=low+(u32)(((u64)(high-low)*(u32)pr)>>18);
        int b=(code<=mid)?1:0;
        if(b) high=mid; else low=mid+1;
        while((low^high)<(1u<<24)){ code=(code<<8)|rd(); low<<=8; high=(high<<8)|0xFF; }
        return b;
    }
};

void encodeBWT(const u8* U,int n,vector<u8>& out){
    Model m; Enc e(out);
    for(int t=0;t<n;t++){
        int c=U[t], f=m.run>2?1:0, ctx=1;
        for(int bi=0;bi<8;bi++){
            int bit=(c>>(7-bi))&1, idx,j; int pr=predict(m,ctx,f,idx,j);
            e.bit(bit,pr); upd(m,ctx,idx,j,bit); ctx=ctx*2+bit;
        }
        m.run=(c==m.c1)?m.run+1:0; m.c2=m.c1; m.c1=c;
    }
    e.flush();
}
void decodeBWT(const u8* in,size_t len,int n,u8* U){
    Model m; Dec d(in,len);
    for(int t=0;t<n;t++){
        int f=m.run>2?1:0, ctx=1;
        for(int bi=0;bi<8;bi++){
            int idx,j; int pr=predict(m,ctx,f,idx,j);
            int bit=d.bit(pr); upd(m,ctx,idx,j,bit); ctx=ctx*2+bit;
        }
        int c=ctx&255; U[t]=(u8)c;
        m.run=(c==m.c1)?m.run+1:0; m.c2=m.c1; m.c1=c;
    }
}

// ---------------- LZP (Lempel-Ziv Prediction) preprocessing ----------------
static const int LZP_ORDER=6, LZP_HBITS=22, LZP_MIN=32;
static inline u32 lzp_hash(const u8* p){
    u32 h=2166136261u; for(int k=0;k<LZP_ORDER;k++) h=(h^p[k])*16777619u;
    return (h ^ (h>>15)) & ((1u<<LZP_HBITS)-1);
}
void lzp_encode(const u8* b,int n,vector<u8>& lit,vector<u32>& tok){
    vector<int32_t> H((size_t)1<<LZP_HBITS,-1); int i=0; u32 run=0;
    while(i<n){
        if(i>=LZP_ORDER){
            u32 h=lzp_hash(b+i-LZP_ORDER); int j=H[h]; H[h]=i;
            if(j>=0){ int L=0,maxL=n-i; while(L<maxL && b[j+L]==b[i+L]) L++;
                if(L>=LZP_MIN){ tok.push_back(run); tok.push_back((u32)L); run=0; i+=L; continue; } }
        }
        lit.push_back(b[i]); run++; i++;
    }
    tok.push_back(run);
}
void lzp_decode(const u8* lit,const u32* tok,int ntok,int n,u8* out){
    vector<int32_t> H((size_t)1<<LZP_HBITS,-1); int i=0; size_t lp=0; int ti=0;
    while(i<n){
        u32 run=tok[ti++];
        for(u32 k=0;k<run;k++){ if(i>=LZP_ORDER){ u32 h=lzp_hash(out+i-LZP_ORDER); H[h]=i; } out[i++]=lit[lp++]; }
        if(ti>=ntok) break;
        u32 L=tok[ti++]; u32 h=lzp_hash(out+i-LZP_ORDER); int j=H[h]; H[h]=i;
        for(u32 k=0;k<L;k++) out[i+k]=out[j+k]; i+=L;
    }
}
void put_varint(vector<u8>& v,u32 x){ while(x>=0x80){ v.push_back((x&0x7f)|0x80); x>>=7; } v.push_back((u8)x); }

int main(int argc,char**argv){
    if(argc<2){ printf("usage: bwtcm <file> [blocksize=0:single|lzp] [nv|maxbytes]\n"); return 1; }
    if(argc>=3 && argv[2][0]=='l' && argv[2][1]=='z'){ // LZP mode: single block of argv[3] bytes
        long long mx=(argc>=4)?atoll(argv[3]):0;
        FILE*fp=fopen(argv[1],"rb"); if(!fp){printf("open fail\n");return 1;}
        fseek(fp,0,SEEK_END); long long fsz=ftell(fp); fseek(fp,0,SEEK_SET);
        if(mx>0&&mx<fsz) fsz=mx;
        int n=(int)fsz; vector<u8> T(n); fread(T.data(),1,n,fp); fclose(fp);
        vector<u8> lit; lit.reserve(n); vector<u32> tok; tok.reserve(n/16);
        lzp_encode(T.data(),n,lit,tok);
        vector<u8> chk(n); lzp_decode(lit.data(),tok.data(),(int)tok.size(),n,chk.data());
        bool lzp_ok=(chk==T);
        int ln=(int)lit.size(); vector<int32_t> A(ln+1);
        libsais_bwt(lit.data(),lit.data(),A.data(),ln,0,nullptr);
        vector<u8> compL; compL.reserve(ln/3); encodeBWT(lit.data(),ln,compL);
        vector<u8> tb; tb.reserve(tok.size()*2); for(u32 x:tok) put_varint(tb,x);
        vector<u8> compT; compT.reserve(tb.size()/2+16); encodeBWT(tb.data(),(int)tb.size(),compT);
        long long total=(long long)compL.size()+compT.size()+16;
        printf("LZP n=%d  lit=%d(%.1f%%) matches=%zu  compLit=%zu compTok=%zu total=%lld  %.4f bpc  lzp_ok=%d\n",
               n,ln,100.0*ln/n,tok.size()/2,compL.size(),compT.size(),total,total*8.0/n,lzp_ok);
        return 0;
    }
    if(argc>=3 && argv[2][0]=='r' && argv[2][1]=='w'){ // raw baseline (first mx bytes, single block)
        long long mx=(argc>=4)?atoll(argv[3]):0;
        FILE*fp=fopen(argv[1],"rb"); fseek(fp,0,SEEK_END); long long fsz=ftell(fp); fseek(fp,0,SEEK_SET);
        if(mx>0&&mx<fsz) fsz=mx; int n=(int)fsz; vector<u8> T(n); fread(T.data(),1,n,fp); fclose(fp);
        vector<int32_t> A(n+1); libsais_bwt(T.data(),T.data(),A.data(),n,0,nullptr);
        vector<u8> c; c.reserve(n/3); encodeBWT(T.data(),n,c);
        printf("RAW     n=%d comp=%zu  %.4f bpc\n",n,c.size(),(c.size()+8)*8.0/n); return 0;
    }
    if(argc>=3 && argv[2][0]=='c' && argv[2][1]=='f'){ // capfold (fold case to a separate stream)
        long long mx=(argc>=4)?atoll(argv[3]):0;
        FILE*fp=fopen(argv[1],"rb"); fseek(fp,0,SEEK_END); long long fsz=ftell(fp); fseek(fp,0,SEEK_SET);
        if(mx>0&&mx<fsz) fsz=mx; int n=(int)fsz; vector<u8> T(n); fread(T.data(),1,n,fp); fclose(fp);
        vector<u8> M(n), cb; int acc=0,bn=0;
        for(int i=0;i<n;i++){ u8 c=T[i]; int up=0; if(c>='A'&&c<='Z'){ c+=32; up=1; }
            if(c>='a'&&c<='z'){ acc|=up<<bn; if(++bn==8){cb.push_back((u8)acc);acc=0;bn=0;} } M[i]=c; }
        if(bn) cb.push_back((u8)acc);
        vector<u8> chk(n); { size_t bi=0; for(int i=0;i<n;i++){ u8 c=M[i]; if(c>='a'&&c<='z'){ int b=(cb[bi>>3]>>(bi&7))&1; bi++; if(b)c-=32; } chk[i]=c; } }
        bool ok=(chk==T);
        vector<int32_t> A(n+1); libsais_bwt(M.data(),M.data(),A.data(),n,0,nullptr);
        vector<u8> cm; cm.reserve(n/3); encodeBWT(M.data(),n,cm);
        vector<u8> cc; cc.reserve(cb.size()/2+16); encodeBWT(cb.data(),(int)cb.size(),cc);
        long long total=(long long)cm.size()+cc.size()+16;
        printf("CAPFOLD n=%d caseB=%zu compMain=%zu compCase=%zu total=%lld  %.4f bpc ok=%d\n",
               n,cb.size(),cm.size(),cc.size(),total,total*8.0/n,ok); return 0;
    }
    long long B = (argc>=3)? atoll(argv[2]) : 0;
    bool verify = !(argc>=4 && argv[3][0]=='n');
    FILE*fp=fopen(argv[1],"rb"); if(!fp){ printf("open fail\n"); return 1; }
    fseek(fp,0,SEEK_END); long long fsz=ftell(fp); fseek(fp,0,SEEK_SET);
    if(B>0 && B<fsz){
        // streaming multi-block: compress the WHOLE file in RAM-sized blocks
        int cap=(int)B; vector<u8> buf(cap); vector<int32_t> A(cap+1);
        long long totalIn=0, totalComp=0; int nb=0;
        auto t0=chrono::steady_clock::now();
        while(totalIn<fsz){
            long long rem=fsz-totalIn; int r=(int)(rem<(long long)cap?rem:(long long)cap);
            fread(buf.data(),1,r,fp);
            int pidx=libsais_bwt(buf.data(),buf.data(),A.data(),r,0,nullptr);
            if(pidx<0){ printf("bwt fail %d\n",pidx); return 1; }
            vector<u8> comp; comp.reserve(r/3); encodeBWT(buf.data(),r,comp);
            totalComp+=(long long)comp.size()+8; totalIn+=r; nb++;
        }
        fclose(fp);
        double ms=chrono::duration<double,milli>(chrono::steady_clock::now()-t0).count();
        printf("FULL FILE n=%lld  %d blocks x %lldMB  comp=%lld B  %.4f bpc  %.0fms\n",
               fsz, nb, B/1000000, totalComp, totalComp*8.0/fsz, ms);
        return 0;
    }
    long long sz=fsz;
    vector<u8> T(sz); fread(T.data(),1,sz,fp); fclose(fp);
    int n=(int)sz;
    vector<int32_t> A(n+1);
    vector<u8> Uvec; u8* U;
    if(verify){ Uvec.resize(n); U=Uvec.data(); } else { U=T.data(); } // in-place BWT saves 1GB
    auto t0=chrono::steady_clock::now();
    int pidx=libsais_bwt(T.data(),U,A.data(),n,0,nullptr);
    if(pidx<0){ printf("bwt fail %d\n",pidx); return 1; }
    auto t1=chrono::steady_clock::now();
    vector<u8> comp; comp.reserve(n/3); encodeBWT(U,n,comp);
    auto t2=chrono::steady_clock::now();
    bool bwt_ok=true, full_ok=true;
    if(verify){
        vector<u8> U2(n); decodeBWT(comp.data(),comp.size(),n,U2.data());
        bwt_ok=(U2==Uvec);
        vector<u8> orig(n); libsais_unbwt(U2.data(),orig.data(),A.data(),n,nullptr,pidx);
        full_ok=(orig==T);
    }
    double bwt_ms=chrono::duration<double,milli>(t1-t0).count();
    double enc_ms=chrono::duration<double,milli>(t2-t1).count();
    size_t total=comp.size()+8; // +n +pidx header
    printf("n=%d  BWT=%.0fms  enc=%.0fms  comp=%zu B (+8 hdr)  %.4f bpc  bwt_ok=%d full_ok=%d\n",
           n, bwt_ms, enc_ms, total, total*8.0/n, bwt_ok, full_ok);
    return 0;
}
