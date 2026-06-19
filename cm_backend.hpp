// cm_backend.hpp — reusable context-mixing entropy backend, extracted from bwtcm.cpp.
// Order-0/1/2 + SSE/APM model + carryless binary range coder. NO BWT, NO libsais — operates directly
// on an arbitrary byte buffer, so mzip can trial it vs ZSTD on residual/columnar streams (keep smaller).
// Self-contained blob: compress() prepends varint(origLen); decompress() reads it back. Per-call model
// state (~146 KB) so every block is independent. Lossless round-trip verified (see CM_BACKEND_TEST).
#ifndef CM_BACKEND_HPP
#define CM_BACKEND_HPP
#include <cstdint>
#include <cstddef>
#include <vector>
#ifdef CM_BACKEND_USE_BWT
#include "libsais.h"   // define CM_BACKEND_USE_BWT + link libsais to enable the BWT+CM path (the residual winner)
#endif

namespace cmbk {
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// ---- bzip3-style model state (order-0/1/2 + 2-level SSE/APM), one per buffer ----
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
    int p=((p0+p1)*7 + 2*p2)>>4;                 // fixed-weight order-0/1/2 blend
    j=p>>12; idx=2*ctx+f;
    int x1=m.C2[idx][j], x2=m.C2[idx][j+1];
    int ssep=x1 + (((x2-x1)*(p&4095))>>12);      // 2-level SSE/APM interpolation
    int pr=ssep*3+p; if(pr<1)pr=1; else if(pr>262143)pr=262143;
    return pr;                                   // 18-bit probability
}
static inline void upd(Model&m,int ctx,int idx,int j,int bit){
    if(bit){ m.C0[ctx]+=(65535-m.C0[ctx])>>2; m.C1[m.c1][ctx]+=(65535-m.C1[m.c1][ctx])>>4;
             m.C2[idx][j]+=(65535-m.C2[idx][j])>>6; m.C2[idx][j+1]+=(65535-m.C2[idx][j+1])>>6; }
    else   { m.C0[ctx]-=m.C0[ctx]>>2; m.C1[m.c1][ctx]-=m.C1[m.c1][ctx]>>4;
             m.C2[idx][j]-=m.C2[idx][j]>>6; m.C2[idx][j+1]-=m.C2[idx][j+1]>>6; }
}

// ---- carryless binary range coder (18-bit probabilities) ----
struct Enc {
    u32 low=0, high=0xFFFFFFFFu; std::vector<u8>& out; Enc(std::vector<u8>&o):out(o){}
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

// ---- core CM on a raw byte buffer (no BWT) ----
static inline void cm_encode(const u8* U,size_t n,std::vector<u8>& out){
    Model m; Enc e(out);
    for(size_t t=0;t<n;t++){
        int c=U[t], f=m.run>2?1:0, ctx=1;
        for(int bi=0;bi<8;bi++){
            int bit=(c>>(7-bi))&1, idx,j; int pr=predict(m,ctx,f,idx,j);
            e.bit(bit,pr); upd(m,ctx,idx,j,bit); ctx=ctx*2+bit;
        }
        m.run=(c==m.c1)?m.run+1:0; m.c2=m.c1; m.c1=c;
    }
    e.flush();
}
static inline void cm_decode(const u8* in,size_t len,size_t n,u8* U){
    Model m; Dec d(in,len);
    for(size_t t=0;t<n;t++){
        int f=m.run>2?1:0, ctx=1;
        for(int bi=0;bi<8;bi++){
            int idx,j; int pr=predict(m,ctx,f,idx,j);
            int bit=d.bit(pr); upd(m,ctx,idx,j,bit); ctx=ctx*2+bit;
        }
        int c=ctx&255; U[t]=(u8)c;
        m.run=(c==m.c1)?m.run+1:0; m.c2=m.c1; m.c1=c;
    }
}

// ---- public, self-describing API (varint(origLen) + CM stream) ----
inline void put_varint(std::vector<u8>& v,u64 x){ while(x>=0x80){ v.push_back((u8)((x&0x7f)|0x80)); x>>=7; } v.push_back((u8)x); }
inline u64 get_varint(const u8* in,size_t len,size_t& pos){
    u64 x=0; int sh=0; while(pos<len){ u8 b=in[pos++]; x|=(u64)(b&0x7f)<<sh; if(!(b&0x80))break; sh+=7; } return x;
}

// Compress buf[0..n) -> self-contained blob.
inline std::vector<u8> compress(const u8* buf,size_t n){
    std::vector<u8> out; out.reserve(n/3+16);
    put_varint(out,(u64)n);
    cm_encode(buf,n,out);
    return out;
}
// Decompress a blob produced by compress() -> original bytes.
inline std::vector<u8> decompress(const u8* in,size_t len){
    size_t pos=0; u64 n=get_varint(in,len,pos);
    std::vector<u8> out((size_t)n);
    if(n) cm_decode(in+pos,len-pos,(size_t)n,out.data());
    return out;
}

#ifdef CM_BACKEND_USE_BWT
// BWT + CM — the winning config for residual/columnar/structured streams (BWT regroups interleaved
// bytes so the order-0/1/2 model can exploit them; CM-alone loses to ZSTD on those, BWT+CM beats it).
// Blob: varint(n) + varint(pidx+1) + CM stream. pidx+1==0 is the sentinel for "BWT failed -> raw CM".
inline std::vector<u8> compress_bwt(const u8* buf,size_t n){
    std::vector<u8> out; out.reserve(n/3+16);
    put_varint(out,(u64)n);
    if(n==0) return out;
    std::vector<u8> bwt(n);
    std::vector<int32_t> A(n+1);
    int pidx = libsais_bwt(buf, bwt.data(), A.data(), (int32_t)n, 0, nullptr);
    if(pidx<0){ put_varint(out,0); cm_encode(buf,n,out); }              // BWT failed -> raw CM
    else      { put_varint(out,(u64)pidx+1); cm_encode(bwt.data(),n,out); }
    return out;
}
inline std::vector<u8> decompress_bwt(const u8* in,size_t len){
    size_t pos=0; u64 n=get_varint(in,len,pos);
    std::vector<u8> out((size_t)n);
    if(n==0) return out;
    u64 pp=get_varint(in,len,pos);
    if(pp==0){ cm_decode(in+pos,len-pos,(size_t)n,out.data()); return out; }   // raw CM (no BWT)
    std::vector<u8> bwt((size_t)n);
    cm_decode(in+pos,len-pos,(size_t)n,bwt.data());
    std::vector<int32_t> A((size_t)n+1);
    libsais_unbwt(bwt.data(), out.data(), A.data(), (int32_t)n, nullptr, (int32_t)(pp-1));
    return out;
}
#endif // CM_BACKEND_USE_BWT

} // namespace cmbk
#endif // CM_BACKEND_HPP

// ---- standalone self-test: g++ -O3 -std=c++17 -DCM_BACKEND_TEST -x c++ cm_backend.hpp -o cmtest ----
#ifdef CM_BACKEND_TEST
#include <cstdio>
#include <cstring>
int main(int argc,char**argv){
    if(argc<2){ printf("usage: cmtest <file>  (roundtrips + reports CM size)\n"); return 1; }
    FILE*fp=fopen(argv[1],"rb"); if(!fp){ printf("open fail\n"); return 1; }
    fseek(fp,0,SEEK_END); long sz=ftell(fp); fseek(fp,0,SEEK_SET);
    std::vector<cmbk::u8> T(sz); if((long)fread(T.data(),1,sz,fp)!=sz){return 1;} fclose(fp);
    auto comp=cmbk::compress(T.data(),T.size());
    auto back=cmbk::decompress(comp.data(),comp.size());
    bool ok=(back==T);
    printf("%s  orig=%ld  CM-alone=%zu  %.4f bpc  roundtrip=%s\n",
           argv[1],sz,comp.size(),comp.size()*8.0/(sz>0?sz:1), ok?"OK":"*** FAIL ***");
#ifdef CM_BACKEND_USE_BWT
    auto cb=cmbk::compress_bwt(T.data(),T.size());
    auto bb=cmbk::decompress_bwt(cb.data(),cb.size());
    bool ok2=(bb==T);
    printf("            BWT+CM=%zu  %.4f bpc  roundtrip=%s\n",
           cb.size(), cb.size()*8.0/(sz>0?sz:1), ok2?"OK":"*** FAIL ***");
    ok = ok && ok2;
#endif
    return ok?0:2;
}
#endif
