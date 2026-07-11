// wikifield.cpp — reversible pre-compression field filter for enwik9-format Wikipedia XML.
//
//   wikifield c <in> <out>   encode (re-render page-id / timestamp / revision-id as deltas/residuals)
//   wikifield d <in> <out>   decode (exact inverse)
//
// Design (frozen): process page-by-page, pass everything outside <page>...</page> verbatim,
// pass any partial/truncated/unparseable page region verbatim.  Each field is rewritten ONLY if
// the inverse re-render reproduces the original bytes exactly (verified at encode time per field).
//   1. page-id   <id>N</id>        -> <id>+D</id>   D = N - prev_page_N (decimal)
//   2. timestamp <timestamp>...</> -> <timestamp>+D</> / -D   D = |S - prev_S| epoch seconds, signed
//   3. rev-id    <id>M</id>        -> <id>~R</id>   R = decimal(zigzag(M - P)), P interpolated from
//                                     an online map of PAST pages' (ts-epoch -> rev-id) pairs.
// Self-describing: original field content starts with a digit; rewritten with '+','-','~'.  The
// decoder dispatches on the first char.  No other bytes anywhere in the file are modified.
//
// Statistics (encoder) printed to stderr when WIKIFIELD_STATS=1.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

using std::string;
using std::vector;
using std::map;

typedef int64_t  i64;
typedef uint64_t u64;

// ------------------------------------------------------------------ substring search
static const size_t NPOS = (size_t)-1;
// find needle in hay[start..hlen)
static size_t sfind(const char* h, size_t hlen, size_t start, const char* ndl, size_t nlen){
    if(nlen==0 || start>hlen || hlen-start<nlen) return NPOS;
    size_t last = hlen - nlen;
    char c0 = ndl[0];
    for(size_t i=start; i<=last; ){
        const void* q = memchr(h+i, c0, last - i + 1);
        if(!q) return NPOS;
        size_t j = (const char*)q - h;
        if(memcmp(h+j, ndl, nlen)==0) return j;
        i = j+1;
    }
    return NPOS;
}

// ------------------------------------------------------------------ decimal helpers
// parse an all-digits [s,e) into v; return false if empty or any non-digit or overflow.
static bool parse_uint(const char* s, size_t n, i64& v){
    if(n==0 || n>18) return false;              // <=18 digits fits i64 safely
    i64 x=0;
    for(size_t i=0;i<n;i++){ char c=s[i]; if(c<'0'||c>'9') return false; x = x*10 + (c-'0'); }
    v=x; return true;
}
static string dec(i64 v){ char b[24]; int n=snprintf(b,sizeof b,"%lld",(long long)v); return string(b,n); }

// zigzag
static u64 zz(i64 x){ return ((u64)x<<1) ^ (u64)(x>>63); }
static i64 unzz(u64 z){ return (i64)(z>>1) ^ -(i64)(z&1); }

// ------------------------------------------------------------------ UTC calendar (Howard Hinnant)
static i64 days_from_civil(i64 y, unsigned m, unsigned d){
    y -= (m<=2);
    i64 era = (y>=0 ? y : y-399) / 400;
    unsigned yoe = (unsigned)(y - era*400);
    unsigned doy = (153*(m + (m>2 ? -3 : 9)) + 2)/5 + d - 1;
    unsigned doe = yoe*365 + yoe/4 - yoe/100 + doy;
    return era*146097 + (i64)doe - 719468;
}
static void civil_from_days(i64 z, i64& y, unsigned& m, unsigned& d){
    z += 719468;
    i64 era = (z>=0 ? z : z-146096) / 146097;
    unsigned doe = (unsigned)(z - era*146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    y = (i64)yoe + era*400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153;
    d = doy - (153*mp+2)/5 + 1;
    m = mp + (mp<10 ? 3 : -9);
    y += (m<=2);
}
// parse canonical "YYYY-MM-DDThh:mm:ssZ" (exactly 20 bytes) -> epoch seconds. false if nonstandard.
static bool parse_ts(const char* s, size_t n, i64& S){
    if(n!=20) return false;
    static const int dig[]={0,1,2,3,5,6,8,9,11,12,14,15,17,18};
    for(int k=0;k<14;k++){ char c=s[dig[k]]; if(c<'0'||c>'9') return false; }
    if(s[4]!='-'||s[7]!='-'||s[10]!='T'||s[13]!=':'||s[16]!=':'||s[19]!='Z') return false;
    int Y=(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+(s[3]-'0');
    int Mo=(s[5]-'0')*10+(s[6]-'0');
    int Da=(s[8]-'0')*10+(s[9]-'0');
    int hh=(s[11]-'0')*10+(s[12]-'0');
    int mi=(s[14]-'0')*10+(s[15]-'0');
    int se=(s[17]-'0')*10+(s[18]-'0');
    if(Mo<1||Mo>12||Da<1||Da>31||hh>23||mi>59||se>59) return false;
    i64 days = days_from_civil(Y,(unsigned)Mo,(unsigned)Da);
    S = days*86400 + hh*3600 + mi*60 + se;
    return true;
}
// render epoch seconds -> canonical 20-byte ISO (no NUL). writes exactly 20 bytes into out.
static void render_ts(i64 S, char out[20]){
    i64 days = S/86400; i64 sod = S%86400;
    if(sod<0){ sod+=86400; days-=1; }
    i64 Y; unsigned Mo,Da; civil_from_days(days,Y,Mo,Da);
    int hh=(int)(sod/3600), mi=(int)((sod%3600)/60), se=(int)(sod%60);
    snprintf(out,21,"%04lld-%02u-%02uT%02u:%02u:%02uZ",(long long)Y,Mo,Da,(unsigned)hh,(unsigned)mi,(unsigned)se);
}

// ------------------------------------------------------------------ rev-id prediction
// P = interpolate current-ts S against the online map of past (ts -> revid) pairs.
static i64 predict(const map<i64,i64>& mp, i64 S){
    if(mp.empty()) return 0;
    auto hi = mp.lower_bound(S);          // first key >= S
    if(hi==mp.end())   return std::prev(hi)->second;   // all keys < S -> nearest = last
    if(hi->first==S)   return hi->second;              // exact match
    if(hi==mp.begin()) return hi->second;              // all keys > S -> nearest = first
    auto lo = std::prev(hi);
    i64 t0=lo->first, r0=lo->second, t1=hi->first, r1=hi->second;
    // linear interpolation, integer, truncating toward zero (deterministic both sides)
    return r0 + (i64)(( (__int128)(r1-r0) * (i64)(S-t0) ) / (i64)(t1-t0));
}

// ------------------------------------------------------------------ field location within a page
struct Field { size_t cs, ce; bool ok; };   // content byte range [cs,ce) inside page, ok=found
// find "<tag>...</tag>" whose OPEN tag begins at/after `from`, within [from,plen).
// returns content range. openlen/closelen are strlen of the tags.
static Field locate(const char* p, size_t plen, size_t from,
                    const char* open, size_t ol, const char* close, size_t cl){
    Field f{0,0,false};
    size_t o = sfind(p,plen,from,open,ol);
    if(o==NPOS) return f;
    size_t cs = o+ol;
    size_t e = sfind(p,plen,cs,close,cl);
    if(e==NPOS) return f;
    f.cs=cs; f.ce=e; f.ok=true; return f;
}

// ------------------------------------------------------------------ shared per-page state
struct State {
    i64 prev_pid=0;
    i64 prev_ts=0;
    map<i64,i64> revmap;
    // stats
    u64 pid_rw=0,pid_vb=0, ts_rw=0,ts_vb=0, rev_rw=0,rev_vb=0, pages=0;
    vector<i64> resid;   // |M-P| for rewritten rev-ids
};

// A substitution within a page: replace content range [cs,ce) with `rep`.
struct Sub { size_t cs, ce; string rep; };

// emit page applying subs (already in ascending, non-overlapping order) then update nothing.
static void emit_page(FILE* out, const char* p, size_t plen, vector<Sub>& subs){
    size_t cur=0;
    for(auto& s: subs){
        if(s.cs>cur) fwrite(p+cur,1,s.cs-cur,out);
        fwrite(s.rep.data(),1,s.rep.size(),out);
        cur = s.ce;
    }
    if(cur<plen) fwrite(p+cur,1,plen-cur,out);
}

// ================================================================== ENCODE
static void encode_page(FILE* out, const char* p, size_t plen, State& st){
    st.pages++;
    // anchors
    size_t tend  = sfind(p,plen,0,"</title>",8);
    size_t rvpos = sfind(p,plen,0,"<revision>",10);
    // page-id: first <id> after </title> and before <revision>
    Field pid{0,0,false};
    if(tend!=NPOS){
        size_t lim = (rvpos!=NPOS)? rvpos : plen;
        size_t o = sfind(p,plen,tend+8,"<id>",4);
        if(o!=NPOS && o<lim){ size_t cs=o+4, e=sfind(p,plen,cs,"</id>",5);
            if(e!=NPOS && e<=lim){ pid.cs=cs; pid.ce=e; pid.ok=true; } }
    }
    // rev-id: first <id> after <revision>
    Field rev{0,0,false};
    if(rvpos!=NPOS) rev = locate(p,plen,rvpos+10,"<id>",4,"</id>",5);
    // timestamp: first <timestamp> after <revision>
    Field ts{0,0,false};
    if(rvpos!=NPOS) ts = locate(p,plen,rvpos+10,"<timestamp>",11,"</timestamp>",12);

    vector<Sub> subs;

    // ---- page-id
    i64 Npid; bool pid_num=false;
    if(pid.ok){
        const char* c=p+pid.cs; size_t n=pid.ce-pid.cs;
        if(parse_uint(c,n,Npid)){
            pid_num=true;
            // no leading zero (else decimal(N)!=orig), and D>0
            bool lz = (n>1 && c[0]=='0');
            i64 D = Npid - st.prev_pid;
            if(!lz && D>0){
                // verify inverse re-render
                if(dec(st.prev_pid+D)==string(c,n)){
                    subs.push_back({pid.cs,pid.ce,"+"+dec(D)});
                    st.pid_rw++;
                } else st.pid_vb++;
            } else st.pid_vb++;
        } else st.pid_vb++;
    }

    // ---- timestamp (compute S; S valid <=> rewritten)
    i64 S=0; bool S_ok=false;
    if(ts.ok){
        const char* c=p+ts.cs; size_t n=ts.ce-ts.cs;
        i64 Sp;
        if(parse_ts(c,n,Sp)){
            char r[21]; render_ts(Sp,r);
            if(memcmp(r,c,20)==0 && n==20){       // re-renders byte-exactly
                S=Sp; S_ok=true;
                i64 D = S - st.prev_ts;
                string rep = (D>=0? "+" : "-") + dec(D>=0? D : -D);
                subs.push_back({ts.cs,ts.ce,rep});
                st.ts_rw++;
            } else st.ts_vb++;
        } else st.ts_vb++;
    }

    // ---- rev-id (needs current S; predict from PAST map, then insert)
    i64 Mrev; bool rev_num=false;
    if(rev.ok){
        const char* c=p+rev.cs; size_t n=rev.ce-rev.cs;
        if(parse_uint(c,n,Mrev)){
            rev_num=true;
            bool lz = (n>1 && c[0]=='0');
            if(S_ok){
                i64 P = predict(st.revmap,S);
                i64 x = Mrev - P;
                if(!lz && dec(P+x)==string(c,n)){
                    subs.push_back({rev.cs,rev.ce,"~"+dec((i64)zz(x))});
                    st.rev_rw++;
                    i64 a = x<0? -x : x; st.resid.push_back(a);
                } else st.rev_vb++;
            } else st.rev_vb++;
        } else st.rev_vb++;
    }

    // ---- state updates (mirror decoder exactly)
    std::sort(subs.begin(),subs.end(),[](const Sub&a,const Sub&b){return a.cs<b.cs;});
    emit_page(out,p,plen,subs);

    if(pid_num)          st.prev_pid = Npid;
    if(S_ok)             st.prev_ts  = S;
    if(S_ok && rev_num)  st.revmap[S] = Mrev;   // insert AFTER predicting
}

// ================================================================== DECODE
static void decode_page(FILE* out, const char* p, size_t plen, State& st){
    size_t tend  = sfind(p,plen,0,"</title>",8);
    size_t rvpos = sfind(p,plen,0,"<revision>",10);
    Field pid{0,0,false};
    if(tend!=NPOS){
        size_t lim = (rvpos!=NPOS)? rvpos : plen;
        size_t o = sfind(p,plen,tend+8,"<id>",4);
        if(o!=NPOS && o<lim){ size_t cs=o+4, e=sfind(p,plen,cs,"</id>",5);
            if(e!=NPOS && e<=lim){ pid.cs=cs; pid.ce=e; pid.ok=true; } }
    }
    Field rev{0,0,false};
    if(rvpos!=NPOS) rev = locate(p,plen,rvpos+10,"<id>",4,"</id>",5);
    Field ts{0,0,false};
    if(rvpos!=NPOS) ts = locate(p,plen,rvpos+10,"<timestamp>",11,"</timestamp>",12);

    vector<Sub> subs;

    // ---- timestamp FIRST (recover S)
    i64 S=0; bool S_ok=false;
    if(ts.ok){
        const char* c=p+ts.cs; size_t n=ts.ce-ts.cs;
        if(n>=1 && (c[0]=='+'||c[0]=='-')){
            i64 D; parse_uint(c+1,n-1,D);
            i64 signedD = (c[0]=='-')? -D : D;
            S = st.prev_ts + signedD; S_ok=true;
            char r[21]; render_ts(S,r);
            subs.push_back({ts.cs,ts.ce,string(r,20)});
        }
        // else verbatim -> S invalid (encoder only wrote +/- when S valid)
    }

    // ---- page-id
    i64 Npid; bool pid_num=false;
    if(pid.ok){
        const char* c=p+pid.cs; size_t n=pid.ce-pid.cs;
        if(n>=1 && c[0]=='+'){
            i64 D; parse_uint(c+1,n-1,D);
            Npid = st.prev_pid + D; pid_num=true;
            subs.push_back({pid.cs,pid.ce,dec(Npid)});
        } else if(parse_uint(c,n,Npid)){
            pid_num=true;                            // verbatim numeric
        }
    }

    // ---- rev-id (uses S from this page's timestamp)
    i64 Mrev; bool rev_num=false;
    if(rev.ok){
        const char* c=p+rev.cs; size_t n=rev.ce-rev.cs;
        if(n>=1 && c[0]=='~'){
            u64 Z; i64 zt; parse_uint(c+1,n-1,zt); Z=(u64)zt;
            i64 x = unzz(Z);
            i64 P = predict(st.revmap,S);            // S must be valid here
            Mrev = P + x; rev_num=true;
            subs.push_back({rev.cs,rev.ce,dec(Mrev)});
        } else if(parse_uint(c,n,Mrev)){
            rev_num=true;                            // verbatim numeric
        }
    }

    std::sort(subs.begin(),subs.end(),[](const Sub&a,const Sub&b){return a.cs<b.cs;});
    emit_page(out,p,plen,subs);

    if(pid_num)          st.prev_pid = Npid;
    if(S_ok)             st.prev_ts  = S;
    if(S_ok && rev_num)  st.revmap[S] = Mrev;
}

// ================================================================== driver
static vector<char> read_file(const char* path){
    FILE* f=fopen(path,"rb"); if(!f){ perror(path); exit(1); }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    vector<char> v((size_t)sz);
    if(sz>0 && fread(v.data(),1,(size_t)sz,f)!=(size_t)sz){ perror("read"); exit(1); }
    fclose(f); return v;
}

int main(int argc,char** argv){
    if(argc!=4 || (strcmp(argv[1],"c")&&strcmp(argv[1],"d"))){
        fprintf(stderr,"usage: %s c|d <in> <out>\n",argv[0]); return 2;
    }
    bool enc = (argv[1][0]=='c');
    vector<char> in = read_file(argv[2]);
    const char* d = in.data(); size_t N = in.size();
    FILE* out=fopen(argv[3],"wb"); if(!out){ perror(argv[3]); return 1; }
    static char obuf[1<<20]; setvbuf(out,obuf,_IOFBF,sizeof obuf);

    State st;
    size_t pos=0;
    while(pos<N){
        size_t p = sfind(d,N,pos,"<page>",6);
        if(p==NPOS){ fwrite(d+pos,1,N-pos,out); break; }
        if(p>pos)  fwrite(d+pos,1,p-pos,out);               // header / between-pages verbatim
        size_t q = sfind(d,N,p,"</page>",7);
        if(q==NPOS){ fwrite(d+p,1,N-p,out); break; }        // truncated page -> verbatim
        size_t qend = q+7;
        if(enc) encode_page(out,d+p,qend-p,st);
        else    decode_page(out,d+p,qend-p,st);
        pos = qend;
    }
    fflush(out); fclose(out);

    if(enc && getenv("WIKIFIELD_STATS")){
        vector<i64>& r = st.resid;
        i64 med=0,p90=0;
        if(!r.empty()){ std::sort(r.begin(),r.end());
            med=r[r.size()/2]; p90=r[(size_t)(r.size()*0.9)]; }
        fprintf(stderr,
          "WFSTATS pages=%llu  pageid rw=%llu vb=%llu  ts rw=%llu vb=%llu  revid rw=%llu vb=%llu  resid n=%zu median=%lld p90=%lld\n",
          (unsigned long long)st.pages,
          (unsigned long long)st.pid_rw,(unsigned long long)st.pid_vb,
          (unsigned long long)st.ts_rw,(unsigned long long)st.ts_vb,
          (unsigned long long)st.rev_rw,(unsigned long long)st.rev_vb,
          r.size(),(long long)med,(long long)p90);
    }
    return 0;
}
