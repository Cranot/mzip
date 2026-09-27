// classify_enwik.cpp — per-byte content classifier for enwik9-format data.
// Usage: classify_enwik <input>
// Writes: <input>.cls (1 byte class-id per input byte), appends to
// classify_counts.txt and classify_check.txt.
//
// Build: g++ -O3 -std=c++17 classify_enwik.cpp -o classify_enwik.exe
//
// Class taxonomy is FROZEN (IDs 0..23) — see below.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <unordered_set>

using std::string;
using std::vector;
using std::unordered_set;

// ---- class ids (FROZEN contract) --------------------------------------------
enum {
    PROSE=0, XML_TAG=1, TITLE=2, PAGE_ID=3, REV_ID=4, TIMESTAMP=5, CONTRIB=6,
    COMMENT=7, REDIR_KEYWORD=8, REDIR_TGT_TITLE=9, REDIR_TGT_OTHER=10,
    LINK_TGT_NEW=11, LINK_TGT_SEEN=12, LINK_DISPLAY=13, TMPL_NAME=14,
    TMPL_FIELD=15, TMPL_VALUE=16, TABLE=17, HEADING=18, ENTITY=19, DIGITS=20,
    CATEGORY=21, CENSUS_PROSE=22, EXTLINK=23, NCLASS=24
};
static const char* NAMES[NCLASS] = {
    "PROSE","XML_TAG","TITLE","PAGE_ID","REV_ID","TIMESTAMP","CONTRIB","COMMENT",
    "REDIR_KEYWORD","REDIR_TGT_TITLE","REDIR_TGT_OTHER","LINK_TGT_NEW",
    "LINK_TGT_SEEN","LINK_DISPLAY","TMPL_NAME","TMPL_FIELD","TMPL_VALUE","TABLE",
    "HEADING","ENTITY","DIGITS","CATEGORY","CENSUS_PROSE","EXTLINK"
};

static const size_t NPOS = (size_t)-1;

// ---- classifier semantic version --------------------------------------------
static const char* CLS_VERSION =
    "classifier v2 (2026-07-11): REV_ID=revision id only; contributor id -> CONTRIB";

// ---- globals ----------------------------------------------------------------
static const uint8_t* B = nullptr;   // input buffer
static uint8_t*       C = nullptr;   // per-byte class array
static size_t         N = 0;         // input size
static unordered_set<string> SEEN;   // global seen-set of normalized titles/targets

// ---- small helpers ----------------------------------------------------------
static inline int lc(int c){ return (c>='A'&&c<='Z') ? c+32 : c; }

// find pattern in B[from,to). Returns index or NPOS.
static size_t findStr(size_t from, size_t to, const char* pat){
    size_t plen = strlen(pat);
    if(plen==0 || from+plen>to) return NPOS;
    uint8_t first = (uint8_t)pat[0];
    size_t i = from;
    while(i+plen<=to){
        const void* p = memchr(B+i, first, to-i);
        if(!p) return NPOS;
        size_t idx = (const uint8_t*)p - B;
        if(idx+plen>to) return NPOS;
        if(memcmp(B+idx, pat, plen)==0) return idx;
        i = idx+1;
    }
    return NPOS;
}

static bool prefixAt(size_t s, size_t e, const char* pat){
    size_t plen = strlen(pat);
    if(s+plen>e) return false;
    return memcmp(B+s, pat, plen)==0;
}

// case-insensitive: does B[s,e) contain w ?
static bool contains_ci(size_t s, size_t e, const char* w){
    size_t m = strlen(w);
    if(m==0) return true;
    if(s+m>e) return false;
    int w0 = lc((unsigned char)w[0]);
    size_t end = e-m;
    for(size_t i=s;i<=end;i++){
        if(lc(B[i])==w0){
            size_t k=1;
            for(;k<m;k++) if(lc(B[i+k])!=lc((unsigned char)w[k])) break;
            if(k==m) return true;
        }
    }
    return false;
}

// normalize a title/target: '_'->' ', trim ws, uppercase first char.
static string normalize(size_t a, size_t b){
    string r;
    r.reserve(b>a? b-a : 0);
    for(size_t i=a;i<b;i++){
        char c = (char)B[i];
        r.push_back(c=='_' ? ' ' : c);
    }
    size_t i=0, j=r.size();
    while(i<j && (unsigned char)r[i]<=' ') i++;
    while(j>i && (unsigned char)r[j-1]<=' ') j--;
    r = r.substr(i, j-i);
    if(!r.empty()) r[0] = (char)toupper((unsigned char)r[0]);
    return r;
}

// casefold + strip spaces/underscores/hyphens (for redirect title-like test)
static string cfstrip(size_t a, size_t b){
    string r; r.reserve(b>a? b-a : 0);
    for(size_t i=a;i<b;i++){
        uint8_t c=B[i];
        if(c==' '||c=='_'||c=='-') continue;
        r.push_back((char)lc(c));
    }
    return r;
}

static int levenshtein(const string& a, const string& b){
    size_t n=a.size(), m=b.size();
    if(n==0) return (int)m;
    if(m==0) return (int)n;
    vector<int> prev(m+1), cur(m+1);
    for(size_t j=0;j<=m;j++) prev[j]=(int)j;
    for(size_t i=1;i<=n;i++){
        cur[0]=(int)i;
        for(size_t j=1;j<=m;j++){
            int cost = (a[i-1]==b[j-1])?0:1;
            int v = prev[j]+1;
            if(cur[j-1]+1 < v) v = cur[j-1]+1;
            if(prev[j-1]+cost < v) v = prev[j-1]+cost;
            cur[j]=v;
        }
        std::swap(prev,cur);
    }
    return prev[m];
}

static bool redirTitleLike(size_t ta, size_t tb, size_t ga, size_t gb){
    // ta,tb = title span ; ga,gb = target span
    string a = cfstrip(ta,tb);
    string b = cfstrip(ga,gb);
    if(a==b) return true;
    if(a.size()<=b.size() && b.compare(0,a.size(),a)==0) return true; // a prefix of b
    if(b.size()<=a.size() && a.compare(0,b.size(),b)==0) return true; // b prefix of a
    if(a==b+"s" || b==a+"s") return true;
    if(levenshtein(a,b)<=2) return true;
    return false;
}

// ---- matching helpers for wiki constructs -----------------------------------
// find matching close for a doubled bracket/brace pair starting at i.
// returns index just past the closing pair, or -1 if not found within limit.
static long findMatch(size_t i, size_t e, char o, char c, size_t limit){
    int depth=0;
    size_t j=i;
    size_t lim = i+limit;
    while(j+1<e){
        if(j>lim) return -1;
        if(B[j]==(uint8_t)o && B[j+1]==(uint8_t)o){ depth++; j+=2; continue; }
        if(B[j]==(uint8_t)c && B[j+1]==(uint8_t)c){ depth--; j+=2; if(depth==0) return (long)j; continue; }
        j++;
    }
    return -1;
}

static void findTableEnd(size_t i, size_t e, size_t& end, bool& closed){
    int depth=0; size_t j=i;
    while(j+1<e){
        if(B[j]=='{'&&B[j+1]=='|'){ depth++; j+=2; continue; }
        if(B[j]=='|'&&B[j+1]=='}'){ depth--; j+=2; if(depth==0){ end=j; closed=true; return; } continue; }
        j++;
    }
    end=e; closed=false;
}

// find top-level (depth 0, ignoring [[..]] {{..}}) '|' in [a,b)
static size_t findTopPipe(size_t a, size_t b){
    int depth=0; size_t j=a;
    while(j<b){
        if(j+1<b){
            if(B[j]=='['&&B[j+1]=='['){depth++;j+=2;continue;}
            if(B[j]==']'&&B[j+1]==']'){if(depth>0)depth--;j+=2;continue;}
            if(B[j]=='{'&&B[j+1]=='{'){depth++;j+=2;continue;}
            if(B[j]=='}'&&B[j+1]=='}'){if(depth>0)depth--;j+=2;continue;}
        }
        if(B[j]=='|'&&depth==0) return j;
        j++;
    }
    return NPOS;
}

// find top-level '=' in [a,b)
static size_t findTopEquals(size_t a, size_t b){
    int depth=0; size_t j=a;
    while(j<b){
        if(j+1<b){
            if(B[j]=='['&&B[j+1]=='['){depth++;j+=2;continue;}
            if(B[j]==']'&&B[j+1]==']'){if(depth>0)depth--;j+=2;continue;}
            if(B[j]=='{'&&B[j+1]=='{'){depth++;j+=2;continue;}
            if(B[j]=='}'&&B[j+1]=='}'){if(depth>0)depth--;j+=2;continue;}
        }
        if(B[j]=='='&&depth==0) return j;
        j++;
    }
    return NPOS;
}

static inline void fill(size_t a, size_t b, uint8_t v){
    for(size_t k=a;k<b;k++) C[k]=v;
}

// ---- wiki markup state machine ----------------------------------------------
static void parseWiki(size_t s, size_t e, uint8_t def);

static void parseLink(size_t s, size_t e){ // B[s..s+2)="[[", B[e-2..e)="]]"
    size_t is=s+2, ie=e-2;
    if(prefixAt(is, ie, "Category:")){
        fill(s, e, CATEGORY);
        return;
    }
    size_t pipe = findTopPipe(is, ie);
    size_t targEnd = (pipe!=NPOS ? pipe : ie);
    string norm = normalize(is, targEnd);
    uint8_t tc = SEEN.count(norm) ? LINK_TGT_SEEN : LINK_TGT_NEW;
    if(pipe!=NPOS){
        fill(s, pipe, tc);                 // "[[" + target
        C[pipe]=LINK_DISPLAY;              // the '|'
        parseWiki(pipe+1, ie, LINK_DISPLAY);
        C[ie]=LINK_DISPLAY; C[ie+1]=LINK_DISPLAY; // "]]"
    } else {
        fill(s, e, tc);                    // "[[" + target + "]]"
    }
    if(!norm.empty()) SEEN.insert(norm);
}

static void parseTemplate(size_t s, size_t e){ // "{{" .. "}}"
    size_t is=s+2, ie=e-2;
    size_t pipe1 = findTopPipe(is, ie);
    size_t nameEnd = (pipe1!=NPOS ? pipe1 : ie);
    fill(s, nameEnd, TMPL_NAME);           // "{{" + name
    if(pipe1==NPOS){ C[ie]=TMPL_NAME; C[ie+1]=TMPL_NAME; return; }
    size_t pos = pipe1;
    uint8_t lastClass = TMPL_VALUE;
    while(pos < ie){
        size_t nextPipe = findTopPipe(pos+1, ie);
        size_t segEnd = (nextPipe!=NPOS ? nextPipe : ie);
        size_t eq = findTopEquals(pos+1, segEnd);
        if(eq!=NPOS){
            fill(pos, eq+1, TMPL_FIELD);   // '|' + name + '='
            parseWiki(eq+1, segEnd, TMPL_VALUE);
        } else {
            C[pos]=TMPL_VALUE;             // '|'
            parseWiki(pos+1, segEnd, TMPL_VALUE);
        }
        lastClass = TMPL_VALUE;
        pos = segEnd;
    }
    C[ie]=lastClass; C[ie+1]=lastClass;    // "}}"
}

static void parseWiki(size_t s, size_t e, uint8_t def){
    size_t i=s;
    while(i<e){
        uint8_t c=B[i];
        // heading: line starts with >=2 '=' and line trimmed ends with >=2 '='
        if(c=='=' && (i==s || B[i-1]=='\n')){
            size_t j=i; while(j<e && B[j]=='=') j++;
            if(j-i>=2){
                size_t le=i; while(le<e && B[le]!='\n') le++;
                size_t k=le; while(k>i && (B[k-1]==' '||B[k-1]=='\t'||B[k-1]=='\r')) k--;
                size_t te=k; while(te>i && B[te-1]=='=') te--;
                if(k-te>=2){ fill(i, le, HEADING); i=le; continue; }
            }
        }
        if(c=='{' && i+1<e){
            if(B[i+1]=='|'){                 // table
                size_t end; bool closed;
                findTableEnd(i, e, end, closed);
                C[i]=TABLE; C[i+1]=TABLE;
                size_t innerEnd = closed ? end-2 : end;
                parseWiki(i+2, innerEnd, TABLE);
                if(closed){ C[end-2]=TABLE; C[end-1]=TABLE; }
                i=end; continue;
            }
            if(B[i+1]=='{'){                 // template
                long end = findMatch(i, e, '{', '}', 65536);
                if(end<0){ C[i]=def; i++; continue; }
                parseTemplate(i, (size_t)end);
                i=(size_t)end; continue;
            }
        }
        if(c=='[' && i+1<e){
            if(B[i+1]=='['){                 // internal link
                long end = findMatch(i, e, '[', ']', 1024);
                if(end<0){ C[i]=def; i++; continue; }
                parseLink(i, (size_t)end);
                i=(size_t)end; continue;
            }
            if(i+5<=e && B[i+1]=='h'&&B[i+2]=='t'&&B[i+3]=='t'&&B[i+4]=='p'){ // external link
                size_t j=i+1; while(j<e && B[j]!=']') j++;
                if(j<e){ fill(i, j+1, EXTLINK); i=j+1; continue; }
            }
        }
        C[i]=def; i++;
    }
}

// ---- overrides (applied over a <text> content region) -----------------------
static void digitsOverride(size_t s, size_t e){
    size_t i=s;
    while(i<e){
        if(B[i]>='0'&&B[i]<='9'){
            size_t j=i; while(j<e && B[j]>='0'&&B[j]<='9') j++;
            if(j-i>=2){
                bool allProse=true;
                for(size_t k=i;k<j;k++) if(C[k]!=PROSE){ allProse=false; break; }
                if(allProse) fill(i, j, DIGITS);
            }
            i=j;
        } else i++;
    }
}

static void entityOverride(size_t s, size_t e){
    size_t i=s;
    while(i<e){
        if(B[i]=='&'){
            size_t maxj = (i+11<e) ? i+11 : e;
            size_t j=i+1; bool ok=false;
            while(j<maxj){
                uint8_t c=B[j];
                if(c==';'){ ok=(j>i+1); break; }
                if(!(isalnum(c)||c=='#')) break;
                j++;
            }
            if(ok){ fill(i, j+1, ENTITY); i=j+1; continue; }
        }
        i++;
    }
}

// ---- redirect handling ------------------------------------------------------
static void classifyRedirect(size_t s, size_t e, size_t titleA, size_t titleB){
    fill(s, e, REDIR_KEYWORD);
    size_t lb = findStr(s, e, "[[");
    if(lb==NPOS) return;                    // no target; all keyword
    long m = findMatch(lb, e, '[', ']', 1<<24);
    size_t rb = (m<0) ? e : (size_t)m;      // rb = just past "]]" or e
    size_t innerEnd = (m<0) ? e : rb-2;     // exclude trailing "]]"
    size_t pipe = findTopPipe(lb+2, innerEnd);
    size_t targEnd = (pipe!=NPOS) ? pipe : innerEnd;
    bool tlike = redirTitleLike(titleA, titleB, lb+2, targEnd);
    fill(lb+2, targEnd, tlike ? REDIR_TGT_TITLE : REDIR_TGT_OTHER);
    string norm = normalize(lb+2, targEnd);
    if(!norm.empty()) SEEN.insert(norm);
}

// ---- <text> region ----------------------------------------------------------
static void classifyText(size_t s, size_t e, size_t titleA, size_t titleB){
    size_t p=s; while(p<e && (unsigned char)B[p]<=' ') p++;
    bool isRedir = false;
    if(p<e && B[p]=='#'){
        // "#REDIRECT" case-insensitive
        const char* kw="redirect";
        if(p+1+8<=e){
            bool ok=true;
            for(int k=0;k<8;k++) if(lc(B[p+1+k])!=kw[k]){ ok=false; break; }
            isRedir=ok;
        }
    }
    bool census=false;
    if(!isRedir)
        census = contains_ci(s,e,"as of the [[census]]") ||
                 (contains_ci(s,e,"census") && contains_ci(s,e,"square mile"));

    if(isRedir) classifyRedirect(s, e, titleA, titleB);
    else        parseWiki(s, e, PROSE);

    digitsOverride(s, e);
    entityOverride(s, e);

    if(census)
        for(size_t k=s;k<e;k++) if(C[k]==PROSE) C[k]=CENSUS_PROSE;
}

// ---- page skeleton ----------------------------------------------------------
static void classifyPage(size_t ps, size_t pe){
    fill(ps, pe, XML_TAG);                  // skeleton default
    bool pageIdSeen=false, inRevision=false, inContributor=false;
    size_t titleA=0, titleB=0;              // page title span (for redirect test)
    bool haveTitle=false;
    size_t i=ps;
    while(i<pe){
        if(B[i]=='<'){
            size_t gt=i+1; while(gt<pe && B[gt]!='>') gt++;
            if(gt>=pe){ i=pe; break; }      // malformed tail
            size_t ns=i+1; bool closeTag=false, selfClose=(B[gt-1]=='/');
            if(ns<gt && B[ns]=='/'){ closeTag=true; ns++; }
            size_t ne=ns;
            while(ne<gt && (isalnum(B[ne])||B[ne]==':'||B[ne]=='-'||B[ne]=='_')) ne++;
            // tag name = B[ns,ne)
            auto nameIs=[&](const char* t)->bool{
                size_t len=strlen(t);
                return (ne-ns)==len && memcmp(B+ns,t,len)==0;
            };
            if(!closeTag && !selfClose){
                if(nameIs("revision")) inRevision=true;
                else if(nameIs("contributor")) inContributor=true;
            }
            if(closeTag){
                if(nameIs("revision")){ inRevision=false; inContributor=false; }
                else if(nameIs("contributor")) inContributor=false;
            }
            if(!closeTag && !selfClose){
                int cls=-1; bool isText=false;
                if(nameIs("title")) cls=TITLE;
                else if(nameIs("timestamp")) cls=TIMESTAMP;
                else if(nameIs("comment")) cls=COMMENT;
                else if(nameIs("username")||nameIs("ip")) cls=CONTRIB;
                else if(nameIs("id")){
                    // v2 semantic split: contributor <id> -> CONTRIB; REV_ID is the
                    // revision-level id only (first id directly inside <revision>).
                    if(inContributor){ cls=CONTRIB; }
                    else if(inRevision){ cls=REV_ID; }
                    else if(!pageIdSeen){ cls=PAGE_ID; pageIdSeen=true; }
                    else cls=REV_ID;
                }
                else if(nameIs("text")){ isText=true; }
                if(cls!=-1 || isText){
                    size_t cs=gt+1;
                    string closeStr = "</" + string((const char*)B+ns, ne-ns) + ">";
                    size_t ce = findStr(cs, pe, closeStr.c_str());
                    if(ce==NPOS) ce=pe;     // unclosed -> to page end
                    if(isText){
                        classifyText(cs, ce, titleA, haveTitle?titleB:titleA);
                    } else {
                        fill(cs, ce, (uint8_t)cls);
                        if(cls==TITLE){
                            titleA=cs; titleB=ce; haveTitle=true;
                            string tn = normalize(cs, ce);
                            if(!tn.empty()) SEEN.insert(tn);
                        }
                    }
                    i=ce; continue;         // close tag reprocessed as XML_TAG
                }
            }
            i=gt+1; continue;
        }
        i++;
    }
}

// ---- driver -----------------------------------------------------------------
static string escExcerpt(size_t a, size_t b){
    // returns readable escaped string of B[a,b)
    string r;
    for(size_t i=a;i<b;i++){
        uint8_t c=B[i];
        if(c=='\n') r+="\\n";
        else if(c=='\r') r+="\\r";
        else if(c=='\t') r+="\\t";
        else if(c<0x20) r+='.';
        else r.push_back((char)c);
    }
    return r;
}

int main(int argc, char** argv){
    if(argc<2){ fprintf(stderr,"usage: %s <input>\n", argv[0]); return 2; }
    printf("%s\n", CLS_VERSION);   // v2 version banner (stdout)
    const char* inpath = argv[1];
    FILE* f=fopen(inpath,"rb");
    if(!f){ fprintf(stderr,"cannot open %s\n", inpath); return 2; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz<0){ fprintf(stderr,"ftell failed\n"); return 2; }
    N=(size_t)sz;
    uint8_t* buf=(uint8_t*)malloc(N?N:1);
    if(!buf){ fprintf(stderr,"oom\n"); return 2; }
    if(N && fread(buf,1,N,f)!=N){ fprintf(stderr,"read failed\n"); return 2; }
    fclose(f);
    B=buf;
    C=(uint8_t*)malloc(N?N:1);
    if(!C){ fprintf(stderr,"oom\n"); return 2; }
    memset(C, XML_TAG, N);

    SEEN.clear();  // fresh per run (single-file invocation)

    // header vs mid-page detection
    size_t p0=0; while(p0<N && (unsigned char)B[p0]<=' ') p0++;
    bool startsHeader = prefixAt(p0, N, "<mediawiki")
                     || prefixAt(p0, N, "<siteinfo")
                     || prefixAt(p0, N, "<page");
    size_t firstPage = findStr(0, N, "<page>");
    bool midPage=false, truncated=false;
    size_t midPrefixBytes=0;

    if(firstPage==NPOS){
        uint8_t def = startsHeader ? XML_TAG : PROSE;
        memset(C, def, N);
        if(!startsHeader){ midPage=true; midPrefixBytes=N; }
    } else {
        uint8_t predef = startsHeader ? XML_TAG : PROSE;
        fill(0, firstPage, predef);
        if(!startsHeader){ midPage=true; midPrefixBytes=firstPage; }
        size_t pos=firstPage;
        while(pos<N){
            size_t pe = findStr(pos, N, "</page>");
            size_t pageEnd = (pe==NPOS) ? N : pe+7;
            if(pe==NPOS) truncated=true;
            classifyPage(pos, pageEnd);
            if(pageEnd>=N) break;
            size_t np = findStr(pageEnd, N, "<page>");
            if(np==NPOS){ fill(pageEnd, N, XML_TAG); break; }
            fill(pageEnd, np, XML_TAG);
            pos=np;
        }
    }

    // ---- write .cls -----------------------------------------------------
    string clspath = string(inpath) + ".cls";
    FILE* fc=fopen(clspath.c_str(),"wb");
    if(!fc){ fprintf(stderr,"cannot write %s\n", clspath.c_str()); return 2; }
    if(N && fwrite(C,1,N,fc)!=N){ fprintf(stderr,"cls write failed\n"); return 2; }
    fclose(fc);

    // ---- counts ---------------------------------------------------------
    unsigned long long cnt[NCLASS]={0};
    unsigned long long content_cnt[NCLASS]={0};  // v2: delimiter-excluded byte count for link/template constructs
    auto isDelimByte=[](uint8_t c)->bool{
        // structural delimiters [[ ]] | {{ }} [ ] = (as individual bytes)
        return c=='['||c==']'||c=='|'||c=='{'||c=='}'||c=='=';
    };
    auto isLinkConstruct=[](int k)->bool{
        return k==LINK_TGT_NEW||k==LINK_TGT_SEEN||k==LINK_DISPLAY||k==CATEGORY||
               k==EXTLINK||k==TMPL_NAME||k==TMPL_FIELD||k==TMPL_VALUE;
    };
    for(size_t i=0;i<N;i++){
        uint8_t cc=C[i];
        cnt[cc]++;
        if(isLinkConstruct(cc) && !isDelimByte(B[i])) content_cnt[cc]++;
    }
    unsigned long long sum=0; for(int k=0;k<NCLASS;k++) sum+=cnt[k];

    FILE* fk=fopen("classify_counts.txt","a");
    fprintf(fk,"=== %s (size %zu bytes) ===\n", inpath, N);
    fprintf(fk,"%s\n", CLS_VERSION);
    fprintf(fk,"content-only (link/template classes): total bytes unchanged; content-only excludes delimiter bytes [ ] | { } =\n");
    if(midPage) fprintf(fk,"note: input starts mid-page; %zu prefix bytes classified as PROSE (0)\n", midPrefixBytes);
    if(truncated) fprintf(fk,"note: trailing page truncated (no </page>); force-closed best-effort\n");
    for(int k=0;k<NCLASS;k++){
        double pct = N? 100.0*(double)cnt[k]/(double)N : 0.0;
        if(isLinkConstruct(k))
            fprintf(fk,"class %2d %-16s %14llu  %7.4f%%   content-only %14llu\n",
                    k, NAMES[k], cnt[k], pct, content_cnt[k]);
        else
            fprintf(fk,"class %2d %-16s %14llu  %7.4f%%\n", k, NAMES[k], cnt[k], pct);
    }
    fprintf(fk,"TOTAL            %-16s %14llu  (sum==size: %s)\n\n", "",
            sum, (sum==N?"YES":"NO"));
    fclose(fk);

    // ---- check samples (up to 3 spread per class) -----------------------
    struct Samp{ size_t start,end; };
    vector<Samp> samp[NCLASS];
    size_t lastStart[NCLASS]; for(int k=0;k<NCLASS;k++) lastStart[k]=NPOS;
    size_t spread = N/5 + 1;
    {
        size_t i=0;
        while(i<N){
            uint8_t c=C[i];
            size_t j=i; while(j<N && C[j]==c) j++;
            size_t len=j-i;
            if(samp[c].size()<3){
                bool spaced = (lastStart[c]==NPOS) || (i>=lastStart[c]+spread);
                bool okLen = (len>=3);
                if(spaced && okLen){
                    samp[c].push_back({i,j});
                    lastStart[c]=i;
                }
            }
            i=j;
        }
        // second pass: any class still empty -> take the longest run of that class
        for(int c=0;c<NCLASS;c++){
            if(!samp[c].empty()) continue;
            size_t bi=NPOS, bl=0;
            for(size_t i=0;i<N;){
                if(C[i]==c){ size_t j=i; while(j<N&&C[j]==c)j++; if(j-i>bl){bl=j-i;bi=i;} i=j; }
                else i++;
            }
            if(bi!=NPOS) samp[c].push_back({bi,bi+bl});
        }
    }

    FILE* fh=fopen("classify_check.txt","a");
    fprintf(fh,"########## %s ##########\n", inpath);
    for(int c=0;c<NCLASS;c++){
        fprintf(fh,"--- class %2d %s : %zu sample(s) ---\n", c, NAMES[c], samp[c].size());
        if(samp[c].empty()){ fprintf(fh,"   (no occurrences)\n"); continue; }
        for(auto& s : samp[c]){
            size_t before = (s.start>=25)? s.start-25 : 0;
            size_t spanEnd = s.end;
            bool trunc = false;
            if(spanEnd - s.start > 60){ spanEnd = s.start+60; trunc=true; }
            size_t after = (spanEnd+20<=N)? spanEnd+20 : N;
            string pre = escExcerpt(before, s.start);
            string span = escExcerpt(s.start, spanEnd);
            string post = escExcerpt(spanEnd, after);
            fprintf(fh,"  @%-10zu len=%zu  %s\xc2\xab%s%s\xc2\xbb%s\n",
                    s.start, s.end-s.start, pre.c_str(), span.c_str(),
                    trunc?"\xe2\x80\xa6":"", post.c_str());
        }
    }
    fprintf(fh,"\n");
    fclose(fh);

    fprintf(stderr,"%s: %zu bytes, sum==size:%s, seen-set=%zu, midpage=%d truncated=%d\n",
            inpath, N, (sum==N?"YES":"NO"), SEEN.size(), (int)midPage, (int)truncated);
    free(buf); free(C);
    return (sum==N)?0:1;
}
