#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
field_probe.py -- analytic entropy/repetition probe for enwik9.

Part 1: structured-field delta analysis (page ids, revision ids, timestamps,
        contributor ids) -- achievable coded size under a reversible delta
        transform, priced with an adaptive Krichevsky-Trofimov (KT) order-0
        byte-entropy estimator, versus the same estimator over the raw field
        bytes as they appear in the file.
Part 2: link-repetition statistics for a candidate "entity trie" model.

Console is cp1253: this script prints ONLY ASCII progress lines to stdout and
writes the full UTF-8 report to field_probe_report.md.

KT note: the online estimator sum_i -log2( (count[b_i]+0.5)/(total_i+128) ) has
an EXACT closed form in terms of the final byte histogram, which we use for
speed (verified against the online loop in _self_test):
    bits = [ lgamma(N+128) - lgamma(128)
             - sum_b ( lgamma(n_b+0.5) - lgamma(0.5) ) ] / ln 2
"""

import sys, os, re, math, calendar, time, bisect

INPUT = sys.argv[1] if len(sys.argv) > 1 else "enwik9"
REPORT = sys.argv[2] if len(sys.argv) > 2 else "field_probe_report.md"
CHUNK = 32 * 1024 * 1024
LN2 = math.log(2.0)

# ----------------------------------------------------------------------------
# KT order-0 estimator (closed form) + online self-test
# ----------------------------------------------------------------------------
_LG_HALF = math.lgamma(0.5)
_LG_128 = math.lgamma(128.0)

def kt_bits_from_hist(hist, N):
    if N <= 0:
        return 0.0
    denom = math.lgamma(N + 128.0) - _LG_128
    numer = 0.0
    for c in hist:
        if c:
            numer += math.lgamma(c + 0.5) - _LG_HALF
    return (denom - numer) / LN2

def kt_bits_online(byte_seq):
    counts = [0] * 256
    total = 0
    bits = 0.0
    for b in byte_seq:
        p = (counts[b] + 0.5) / (total + 128.0)
        bits += -math.log2(p)
        counts[b] += 1
        total += 1
    return bits

def hist_of(buf):
    """256-bin histogram of a bytes/bytearray, C-speed via .count()."""
    h = [0] * 256
    for i in range(256):
        c = buf.count(i)
        if c:
            h[i] = c
    return h, len(buf)

def kt_bytes(buf):
    h, N = hist_of(buf)
    return kt_bits_from_hist(h, N) / 8.0, N

def _self_test():
    import random
    random.seed(1)
    seq = bytes(random.randint(0, 255) for _ in range(3000))
    on = kt_bits_online(seq)
    h, N = hist_of(seq)
    cf = kt_bits_from_hist(h, N)
    assert abs(on - cf) < 1e-6, (on, cf)
    # small skewed stream too
    seq2 = bytes([0, 0, 0, 1, 0, 2, 0, 0, 1, 0] * 40)
    assert abs(kt_bits_online(seq2) - kt_bits_from_hist(*hist_of(seq2))) < 1e-6
    print("SELFTEST kt closed-form == online: PASS")

# ----------------------------------------------------------------------------
# varint helpers
# ----------------------------------------------------------------------------
def zigzag(n):
    return (n << 1) if n >= 0 else (((-n) << 1) - 1)

def uvarint_into(u, out):
    while True:
        b = u & 0x7F
        u >>= 7
        if u:
            out.append(b | 0x80)
        else:
            out.append(b)
            return

def delta_varint_stream(values):
    """prev-init-0 delta, zigzag, LEB128 -> bytearray (fully reconstructs)."""
    out = bytearray()
    prev = 0
    for v in values:
        uvarint_into(zigzag(v - prev), out)
        prev = v
    return out

def raw_varint_stream(values):
    out = bytearray()
    for v in values:
        uvarint_into(v, out)  # values are non-negative here
    return out

def monotonicity(values):
    n = len(values)
    if n < 2:
        return (0.0, 0, 0, 0, n)
    pos = neg = zero = 0
    prev = values[0]
    for v in values[1:]:
        d = v - prev
        if d > 0:
            pos += 1
        elif d < 0:
            neg += 1
        else:
            zero += 1
        prev = v
    tot = n - 1
    return (100.0 * pos / tot, pos, neg, zero, n)

# ----------------------------------------------------------------------------
# timestamp parse (fast, manual)
# ----------------------------------------------------------------------------
def parse_ts(s):
    # 2005-12-27T18:46:47Z
    try:
        y = int(s[0:4]); mo = int(s[5:7]); d = int(s[8:10])
        H = int(s[11:13]); M = int(s[14:16]); S = int(s[17:19])
        return calendar.timegm((y, mo, d, H, M, S, 0, 0, 0))
    except Exception:
        return None

# ----------------------------------------------------------------------------
# link-target normalization + redirect title-like test
# ----------------------------------------------------------------------------
def norm_target(t):
    t = t.strip().replace("_", " ").strip()
    if not t:
        return ""
    return t[0].upper() + t[1:]

_STRIP = str.maketrans("", "", " -_")
def cmp_key(x):
    return x.casefold().translate(_STRIP)

def lev_le2(a, b):
    la, lb = len(a), len(b)
    if abs(la - lb) > 2:
        return False
    prev = list(range(lb + 1))
    for i in range(1, la + 1):
        cur = [i] + [0] * lb
        mn = cur[0]
        ca = a[i - 1]
        for j in range(1, lb + 1):
            cost = 0 if ca == b[j - 1] else 1
            v = prev[j] + 1
            w = cur[j - 1] + 1
            if w < v:
                v = w
            w = prev[j - 1] + cost
            if w < v:
                v = w
            cur[j] = v
            if v < mn:
                mn = v
        if mn > 2:
            return False
        prev = cur
    return prev[lb] <= 2

def title_like(title, target):
    kt = cmp_key(title)
    kr = cmp_key(target)
    if not kt or not kr:
        return False
    if kt == kr:
        return True
    if kr.startswith(kt) or kt.startswith(kr):
        return True
    if kr == kt + "s" or kt == kr + "s":
        return True
    return lev_le2(kt, kr)

# ----------------------------------------------------------------------------
# timestamp -> revision-id online regression (decoder-mirrorable: predicts the
# current rev-id from PAST (ts, rid) pairs only, then inserts the pair)
# ----------------------------------------------------------------------------
def _med(v):
    s = sorted(v)
    n = len(s)
    return s[n // 2] if n & 1 else 0.5 * (s[n // 2 - 1] + s[n // 2])

def _local_slope(reg_ts, reg_rid, a, b):
    dt = reg_ts[b] - reg_ts[a]
    if dt <= 0:
        return 0.0
    return (reg_rid[b] - reg_rid[a]) / dt

def predict_rid(reg_ts, reg_rid, ts, k):
    """Predict rid at ts from past pairs sorted by ts.
    k=1: linear interp between the bracketing neighbours.
    k>1: linear interp between (median ts, median rid) of up to k
         neighbours on each side (robust to noise)."""
    n = len(reg_ts)
    if n == 0:
        return 0
    i = bisect.bisect_left(reg_ts, ts)
    lo0, lo1 = max(0, i - k), i
    hi0, hi1 = i, min(n, i + k)
    have_lo = lo1 > lo0
    have_hi = hi1 > hi0
    if have_lo and have_hi:
        if k == 1:
            t_b, r_b = reg_ts[i - 1], reg_rid[i - 1]
            t_a, r_a = reg_ts[i], reg_rid[i]
        else:
            t_b = _med(reg_ts[lo0:lo1]); r_b = _med(reg_rid[lo0:lo1])
            t_a = _med(reg_ts[hi0:hi1]); r_a = _med(reg_rid[hi0:hi1])
        if t_a <= t_b:
            return int(round((r_b + r_a) / 2.0))
        return int(round(r_b + (r_a - r_b) * (ts - t_b) / (t_a - t_b)))
    if have_lo:  # ts beyond all past: extrapolate with recent local slope
        j = n - 1
        a = max(0, n - 64)
        sl = _local_slope(reg_ts, reg_rid, a, j) if j > a else 0.0
        return max(0, int(round(reg_rid[j] + sl * (ts - reg_ts[j]))))
    # ts before all past
    a2 = min(n - 1, 63)
    sl = _local_slope(reg_ts, reg_rid, 0, a2) if a2 > 0 else 0.0
    return max(0, int(round(reg_rid[0] - sl * (reg_ts[0] - ts))))

def ts_rid_regression(pairs, k):
    """Online simulation over (ts, rid) pairs in page order.
    Returns (residual list, zigzag-varint residual bytearray)."""
    reg_ts = []
    reg_rid = []
    residuals = []
    out = bytearray()
    for ts, rid in pairs:
        pred = predict_rid(reg_ts, reg_rid, ts, k)
        r = rid - pred
        residuals.append(r)
        uvarint_into(zigzag(r), out)
        j = bisect.bisect_left(reg_ts, ts)
        reg_ts.insert(j, ts)
        reg_rid.insert(j, rid)
    return residuals, out

def abs_percentiles(residuals):
    """(median, p90, p99, max, mean) of |residual|."""
    if not residuals:
        return (0, 0, 0, 0, 0.0)
    a = sorted(abs(r) for r in residuals)
    n = len(a)
    def pc(q):
        return a[min(n - 1, int(q * n))]
    return (pc(0.50), pc(0.90), pc(0.99), a[-1], sum(a) / n)

# ----------------------------------------------------------------------------
# main streaming pass
# ----------------------------------------------------------------------------
LINK_RE = re.compile(r"\[\[(.*?)\]\]", re.DOTALL)
# interwiki language-link prefix: 2-3 lowercase letters, up to two -suffixes
# (fr, de, zh-min-nan, roa-rup, ...). NOTE: excludes long codes like
# 'simple'/'minnan'/'tokipona' per the 2-3-letter spec.
IW_RE = re.compile(r"^([a-z]{2,3}(?:-[a-z]+){0,2}):(.+)$", re.DOTALL)
ID_RE = re.compile(r"^<id>(\d+)</id>$")
TITLE_RE = re.compile(r"^<title>(.*)</title>$")
TS_RE = re.compile(r"^<timestamp>(.*)</timestamp>$")

LENMAX = 512  # target-length histogram cap

def main():
    t0 = time.time()
    _self_test()

    total_size = os.path.getsize(INPUT)
    print("INPUT %s  size=%d" % (INPUT, total_size))

    # Part 1 sequences + raw byte buffers
    page_ids = []
    rev_ids = []
    ts_epochs = []
    contrib_ids = []
    raw_pid = bytearray()
    raw_rid = bytearray()
    raw_ts = bytearray()
    raw_cid = bytearray()

    page_count = 0
    page_open_tags = 0
    title_tags = 0
    pages_without_contrib = 0

    # Part 2 structures
    target_counts = {}      # normalized target -> occurrence count (big)
    title_set = set()       # normalized page titles seen so far
    cat_counts = {}         # normalized category target -> count
    len_hist = [0] * (LENMAX + 1)      # all-links target byte-length hist
    cat_len_hist = [0] * (LENMAX + 1)

    link_occ = 0
    link_target_bytes = 0
    seen_occ = 0
    seen_bytes = 0

    cat_occ = 0
    cat_target_bytes = 0
    cat_seen_occ = 0
    cat_seen_bytes = 0

    redirect_count = 0
    redirect_with_target = 0
    redirect_titlelike = 0

    # Part 3: (ts, rid) pairs in page order for online regression
    reg_pairs = []

    # Part 4: interwiki [[xx:Title]] stats
    iw_occ = 0
    iw_bytes = 0
    iw_own_exact = 0
    iw_own_key = 0
    iw_sibling = 0
    iw_sibling_exact = 0   # informational (exact-string sibling repeats)
    iw_resid = 0
    iw_langs = {}

    # Part 5: Rambot census pages
    census_pages = 0
    census_bytes = 0

    # current-page holders
    cur_title = None
    cur_pid = cur_rid = cur_ts = cur_cid = None
    cur_pid_raw = cur_rid_raw = cur_ts_raw = cur_cid_raw = None
    in_rev = False
    in_contrib = False
    page_open = False

    # text accumulation
    in_text = False
    text_parts = None

    def process_text(text):
        nonlocal link_occ, link_target_bytes, seen_occ, seen_bytes
        nonlocal cat_occ, cat_target_bytes, cat_seen_occ, cat_seen_bytes
        nonlocal redirect_count, redirect_with_target, redirect_titlelike
        nonlocal iw_occ, iw_bytes, iw_own_exact, iw_own_key, iw_sibling
        nonlocal iw_sibling_exact, iw_resid, census_pages, census_bytes
        first_target = None
        iw_list = []
        for m in LINK_RE.finditer(text):
            inner = m.group(1)
            raw_t = inner.split("|", 1)[0]
            im = IW_RE.match(raw_t.strip())
            if im:
                ft = im.group(2).strip()
                if ft:
                    iw_list.append(ft)
                    lang = im.group(1)
                    iw_langs[lang] = iw_langs.get(lang, 0) + 1
            norm = norm_target(raw_t)
            if not norm:
                continue
            blen = len(raw_t)  # bytes (latin-1: 1 char == 1 byte)
            link_occ += 1
            link_target_bytes += blen
            len_hist[blen if blen <= LENMAX else LENMAX] += 1
            seen = (norm in target_counts) or (norm in title_set)
            if seen:
                seen_occ += 1
                seen_bytes += blen
            target_counts[norm] = target_counts.get(norm, 0) + 1
            if first_target is None:
                first_target = norm
            if norm.startswith("Category:"):
                cat_occ += 1
                cat_target_bytes += blen
                cat_len_hist[blen if blen <= LENMAX else LENMAX] += 1
                cseen = (norm in cat_counts) or (norm in title_set)
                if cseen:
                    cat_seen_occ += 1
                    cat_seen_bytes += blen
                cat_counts[norm] = cat_counts.get(norm, 0) + 1
        # interwiki classification (hierarchical, mutually exclusive):
        # exact own title > casefold/strip own title > sibling repeat > residual
        if iw_list:
            iw_occ += len(iw_list)
            keys = [cmp_key(t) for t in iw_list]
            kc = {}
            for k in keys:
                kc[k] = kc.get(k, 0) + 1
            ec = {}
            for t in iw_list:
                ec[t] = ec.get(t, 0) + 1
            tkey = cmp_key(cur_title) if cur_title else ""
            for t, k in zip(iw_list, keys):
                iw_bytes += len(t)
                own = False
                if cur_title is not None and t == cur_title:
                    iw_own_exact += 1
                    own = True
                elif tkey and k == tkey:
                    iw_own_key += 1
                    own = True
                elif kc[k] >= 2:
                    iw_sibling += 1
                else:
                    iw_resid += 1
                if not own and ec[t] >= 2:
                    iw_sibling_exact += 1
        # Rambot census articles
        if "As of the [[census]]" in text:
            census_pages += 1
            census_bytes += len(text)
        # redirect detection
        st = text.lstrip()
        if st[:9].upper() == "#REDIRECT":
            redirect_count += 1
            if first_target is not None:
                redirect_with_target += 1
                if cur_title is not None and title_like(cur_title, first_target):
                    redirect_titlelike += 1

    def finish_page():
        nonlocal page_count, pages_without_contrib, page_open
        if cur_pid is not None:
            page_ids.append(cur_pid); raw_pid.extend(cur_pid_raw)
        if cur_rid is not None:
            rev_ids.append(cur_rid); raw_rid.extend(cur_rid_raw)
        if cur_ts is not None:
            ts_epochs.append(cur_ts); raw_ts.extend(cur_ts_raw)
        if cur_cid is not None:
            contrib_ids.append(cur_cid); raw_cid.extend(cur_cid_raw)
        else:
            pages_without_contrib += 1
        if cur_ts is not None and cur_rid is not None:
            reg_pairs.append((cur_ts, cur_rid))
        page_count += 1
        page_open = False

    f = open(INPUT, "rb")
    buf = ""
    processed = 0
    next_report = 64 * 1024 * 1024
    while True:
        chunk = f.read(CHUNK)
        if not chunk:
            break
        processed += len(chunk)
        data = buf + chunk.decode("latin-1")
        lines = data.split("\n")
        buf = lines.pop()  # trailing partial line
        for line in lines:
            if in_text:
                end = line.find("</text>")
                if end != -1:
                    text_parts.append(line[:end])
                    in_text = False
                    process_text("\n".join(text_parts))
                    text_parts = None
                else:
                    text_parts.append(line)
                continue
            s = line.strip()
            if not s:
                continue
            c0 = s[0]
            if c0 != "<":
                continue
            # --- text open ---
            if s.startswith("<text"):
                gt = line.find(">")
                if gt != -1 and line[gt - 1] == "/":
                    process_text("")  # self-closing empty text
                else:
                    rest = line[gt + 1:]
                    end = rest.find("</text>")
                    if end != -1:
                        process_text(rest[:end])
                    else:
                        in_text = True
                        text_parts = [rest]
                continue
            # --- id ---
            if s.startswith("<id>"):
                m = ID_RE.match(s)
                if m:
                    v = m.group(1)
                    if in_contrib:
                        if cur_cid is None:
                            cur_cid = int(v); cur_cid_raw = v.encode("latin-1")
                    elif in_rev:
                        if cur_rid is None:
                            cur_rid = int(v); cur_rid_raw = v.encode("latin-1")
                    else:
                        if cur_pid is None:
                            cur_pid = int(v); cur_pid_raw = v.encode("latin-1")
                continue
            # --- title (new page) ---
            if s.startswith("<title>"):
                m = TITLE_RE.match(s)
                if m:
                    title_tags += 1
                    cur_title = m.group(1)
                    title_set.add(norm_target(cur_title))
                    cur_pid = cur_rid = cur_ts = cur_cid = None
                    cur_pid_raw = cur_rid_raw = cur_ts_raw = cur_cid_raw = None
                    in_rev = False
                    in_contrib = False
                    page_open = True
                continue
            # --- timestamp ---
            if s.startswith("<timestamp>"):
                m = TS_RE.match(s)
                if m:
                    raw = m.group(1)
                    e = parse_ts(raw)
                    if e is not None:
                        cur_ts = e; cur_ts_raw = raw.encode("latin-1")
                continue
            # --- structural ---
            if s == "<revision>":
                in_rev = True
                continue
            if s == "<contributor>":
                in_contrib = True
                continue
            if s == "</contributor>":
                in_contrib = False
                continue
            if s == "<page>":
                page_open_tags += 1
                continue
            if s == "</page>":
                if page_open:
                    finish_page()
                continue

        if processed >= next_report:
            print("PROGRESS %d MB / %d MB  pages=%d links=%d distinct=%d  %.0fs"
                  % (processed // (1024 * 1024), total_size // (1024 * 1024),
                     page_count, link_occ, len(target_counts), time.time() - t0))
            next_report += 64 * 1024 * 1024

    f.close()
    # tail: finalize any in-progress (truncated) page
    if page_open and cur_pid is not None:
        finish_page()
        print("NOTE finalized truncated tail page")

    print("PASS DONE  %.0fs  pages=%d  links=%d  distinct_targets=%d"
          % (time.time() - t0, page_count, link_occ, len(target_counts)))

    # ------------------------------------------------------------------
    # Part 1 analysis
    # ------------------------------------------------------------------
    def field_stats(name, values, raw_buf, signed=True):
        raw_kt_bytes, raw_N = kt_bytes(bytes(raw_buf))
        dstream = delta_varint_stream(values)
        del_kt_bytes, del_N = kt_bytes(bytes(dstream))
        mono = monotonicity(values)
        return {
            "name": name,
            "count": len(values),
            "raw_bytes": len(raw_buf),
            "raw_kt": raw_kt_bytes,
            "del_varint_bytes": len(dstream),
            "del_kt": del_kt_bytes,
            "mono_pct": mono[0],
            "mono_pos": mono[1],
            "mono_neg": mono[2],
            "mono_zero": mono[3],
        }

    print("Part1: page ids ...")
    fs_pid = field_stats("page id", page_ids, raw_pid)
    print("Part1: revision ids ...")
    fs_rid = field_stats("revision id", rev_ids, raw_rid)
    print("Part1: contributor ids ...")
    fs_cid = field_stats("contributor id", contrib_ids, raw_cid)
    print("Part1: timestamps ...")
    fs_ts = field_stats("timestamp", ts_epochs, raw_ts)

    # timestamp special: second-delta vs day+time-of-day split
    sec_delta_stream = delta_varint_stream(ts_epochs)
    sec_kt, _ = kt_bytes(bytes(sec_delta_stream))
    days = [e // 86400 for e in ts_epochs]
    tods = [e % 86400 for e in ts_epochs]
    day_delta_stream = delta_varint_stream(days)
    tod_stream = raw_varint_stream(tods)
    day_kt, _ = kt_bytes(bytes(day_delta_stream))
    tod_kt, _ = kt_bytes(bytes(tod_stream))
    split_kt = day_kt + tod_kt
    ts_best = "second-delta" if sec_kt <= split_kt else "day+time-of-day split"
    ts_best_bytes = min(sec_kt, split_kt)

    # Part 3: timestamp -> rev-id online regression, two mirrorable variants
    print("Part3: ts->rid regression k=1 (bracket-NN interp) ...")
    res1, stream1 = ts_rid_regression(reg_pairs, 1)
    kt_res1, _ = kt_bytes(bytes(stream1))
    st1 = abs_percentiles(res1)
    print("Part3: ts->rid regression k=4 (median-of-4 interp) ...")
    res4, stream4 = ts_rid_regression(reg_pairs, 4)
    kt_res4, _ = kt_bytes(bytes(stream4))
    st4 = abs_percentiles(res4)
    if kt_res4 <= kt_res1:
        reg_best_name, reg_best_kt, reg_best_st = "median-of-4 interp", kt_res4, st4
    else:
        reg_best_name, reg_best_kt, reg_best_st = "bracket-NN interp", kt_res1, st1

    # ------------------------------------------------------------------
    # Part 2 analysis
    # ------------------------------------------------------------------
    def len_stats(hist):
        tot = sum(hist)
        if tot == 0:
            return (0.0, 0.0, 0)
        s = sum(i * hist[i] for i in range(len(hist)))
        mean = s / tot
        half = tot / 2.0
        acc = 0
        med = 0
        for i in range(len(hist)):
            acc += hist[i]
            if acc >= half:
                med = i
                break
        return (mean, float(med), tot)

    all_mean, all_med, _ = len_stats(len_hist)
    cat_mean, cat_med, _ = len_stats(cat_len_hist)

    # top-20 targets
    top20 = sorted(target_counts.items(), key=lambda kv: kv[1], reverse=True)[:20]
    cat_top20 = sorted(cat_counts.items(), key=lambda kv: kv[1], reverse=True)[:20]

    distinct_targets = len(target_counts)
    distinct_cats = len(cat_counts)

    # ------------------------------------------------------------------
    # write report
    # ------------------------------------------------------------------
    def bpct(part, whole):
        return (100.0 * part / whole) if whole else 0.0

    L = []
    W = L.append
    W("# enwik9 field-probe report\n")
    W("Generated by `field_probe.py` over `%s` (%d bytes). "
      "Elapsed %.0fs.\n" % (INPUT, total_size, time.time() - t0))
    W("KT = adaptive order-0 Krichevsky-Trofimov coder cost "
      "`(count[b]+0.5)/(total+128)`, reported in **bytes** (closed-form, "
      "verified == online).\n")

    W("\n## Page sanity\n")
    W("| metric | value |")
    W("|---|---:|")
    W("| `<page>` open tags | %d |" % page_open_tags)
    W("| `<title>` tags | %d |" % title_tags)
    W("| finalized pages | %d |" % page_count)
    W("| pages without contributor id (ip/none) | %d |" % pages_without_contrib)
    W("| page count vs expected ~243k | %s |"
      % ("OK" if 220000 <= page_count <= 270000 else "CHECK"))

    W("\n## Part 1 -- structured fields: raw vs delta-varint KT (bytes)\n")
    W("| field | count | raw bytes | KT(raw) | delta-varint bytes | "
      "KT(delta) | max savings (KT_raw-KT_delta) | savings % | mono %+ |")
    W("|---|---:|---:|---:|---:|---:|---:|---:|---:|")
    for fs in (fs_pid, fs_rid, fs_cid, fs_ts):
        sav = fs["raw_kt"] - fs["del_kt"]
        W("| %s | %d | %d | %.0f | %d | %.0f | %.0f | %.1f%% | %.3f%% |"
          % (fs["name"], fs["count"], fs["raw_bytes"], fs["raw_kt"],
             fs["del_varint_bytes"], fs["del_kt"], sav,
             bpct(sav, fs["raw_kt"]), fs["mono_pct"]))

    W("\n### Monotonicity detail (consecutive deltas)\n")
    W("| field | count | %+ deltas | positive | negative | zero |")
    W("|---|---:|---:|---:|---:|---:|")
    for fs in (fs_pid, fs_rid, fs_cid, fs_ts):
        W("| %s | %d | %.3f%% | %d | %d | %d |"
          % (fs["name"], fs["count"], fs["mono_pct"],
             fs["mono_pos"], fs["mono_neg"], fs["mono_zero"]))

    W("\n### Timestamp special: second-delta vs day+time-of-day split (KT bytes)\n")
    W("| encoding | KT bytes |")
    W("|---|---:|")
    W("| second-delta varint | %.0f |" % sec_kt)
    W("| day-delta varint | %.0f |" % day_kt)
    W("| time-of-day varint | %.0f |" % tod_kt)
    W("| day+time-of-day split (sum) | %.0f |" % split_kt)
    W("| **smaller** | **%s = %.0f** |" % (ts_best, ts_best_bytes))
    W("\nRaw timestamp KT baseline: %.0f bytes; best delta model saves %.0f "
      "bytes (%.1f%%).\n"
      % (fs_ts["raw_kt"], fs_ts["raw_kt"] - ts_best_bytes,
         bpct(fs_ts["raw_kt"] - ts_best_bytes, fs_ts["raw_kt"])))

    W("\n## Part 2 -- link-repetition statistics (all `[[...]]`)\n")
    W("| metric | value |")
    W("|---|---:|")
    W("| total link occurrences | %d |" % link_occ)
    W("| total bytes inside targets | %d |" % link_target_bytes)
    W("| distinct normalized targets | %d |" % distinct_targets)
    W("| occurrences seen-before (target or earlier title) | %d (%.2f%%) |"
      % (seen_occ, bpct(seen_occ, link_occ)))
    W("| bytes in seen-before occurrences (trie addressable mass) | %d (%.2f%%) |"
      % (seen_bytes, bpct(seen_bytes, link_target_bytes)))
    W("| page titles in title set | %d |" % len(title_set))
    W("| mean / median target length (bytes) | %.2f / %.0f |" % (all_mean, all_med))

    W("\n### Top-20 targets (all links)\n")
    W("| rank | target | count |")
    W("|---:|---|---:|")
    for i, (tg, c) in enumerate(top20, 1):
        safe = tg.replace("|", "\\|")
        if len(safe) > 60:
            safe = safe[:60] + "..."
        W("| %d | %s | %d |" % (i, safe, c))

    W("\n### Category links `[[Category:...]]` (subset of above)\n")
    W("| metric | value |")
    W("|---|---:|")
    W("| category occurrences | %d |" % cat_occ)
    W("| total bytes inside category targets | %d |" % cat_target_bytes)
    W("| distinct category targets | %d |" % distinct_cats)
    W("| occurrences seen-before | %d (%.2f%%) |"
      % (cat_seen_occ, bpct(cat_seen_occ, cat_occ)))
    W("| bytes in seen-before category occurrences | %d (%.2f%%) |"
      % (cat_seen_bytes, bpct(cat_seen_bytes, cat_target_bytes)))
    W("| mean / median category target length (bytes) | %.2f / %.0f |"
      % (cat_mean, cat_med))
    W("\n#### Top-20 category targets\n")
    W("| rank | target | count |")
    W("|---:|---|---:|")
    for i, (tg, c) in enumerate(cat_top20, 1):
        safe = tg.replace("|", "\\|")
        if len(safe) > 60:
            safe = safe[:60] + "..."
        W("| %d | %s | %d |" % (i, safe, c))

    W("\n### Redirect pages (`#REDIRECT` at text start, case-insensitive)\n")
    W("| metric | value |")
    W("|---|---:|")
    W("| redirect pages | %d |" % redirect_count)
    W("| redirects with a resolvable `[[target]]` | %d |" % redirect_with_target)
    W("| target title-like vs own title | %d |" % redirect_titlelike)
    W("| %% title-like (of redirects with target) | %.2f%% |"
      % bpct(redirect_titlelike, redirect_with_target))
    W("| %% title-like (of all redirects) | %.2f%% |"
      % bpct(redirect_titlelike, redirect_count))
    W("\nTitle-like test = casefold + strip spaces/hyphens/underscores, then "
      "equal OR prefix (either direction) OR plural-s OR Levenshtein<=2.\n")

    # ------------------------------------------------------------------
    # Part 3 -- ts -> rev-id online regression
    # ------------------------------------------------------------------
    W("\n## Part 3 -- timestamp -> revision-id online regression "
      "(decoder-mirrorable)\n")
    W("Predictor sees only PAST (timestamp, rev-id) pairs (in page order), "
      "predicts the current rev-id from the timestamp, then inserts the pair "
      "into a ts-sorted map. Residual = actual - predicted, coded as "
      "zigzag varint; cost = KT order-0 over the residual byte stream. "
      "Pairs: %d.\n" % len(reg_pairs))
    W("| predictor | residual varint bytes | KT bytes | vs KT(delta-varint) "
      "%.0f | vs KT(raw) %.0f |" % (fs_rid["del_kt"], fs_rid["raw_kt"]))
    W("|---|---:|---:|---:|---:|")
    W("| bracket-NN interp (k=1) | %d | %.0f | %+.1f%% | %+.1f%% |"
      % (len(stream1), kt_res1,
         100.0 * (kt_res1 - fs_rid["del_kt"]) / fs_rid["del_kt"],
         100.0 * (kt_res1 - fs_rid["raw_kt"]) / fs_rid["raw_kt"]))
    W("| median-of-4 interp (k=4) | %d | %.0f | %+.1f%% | %+.1f%% |"
      % (len(stream4), kt_res4,
         100.0 * (kt_res4 - fs_rid["del_kt"]) / fs_rid["del_kt"],
         100.0 * (kt_res4 - fs_rid["raw_kt"]) / fs_rid["raw_kt"]))
    W("\n**Best: %s = %.0f KT bytes** (saves %.0f B / %.1f%% vs the "
      "delta-varint baseline, %.0f B / %.1f%% vs KT-raw).\n"
      % (reg_best_name, reg_best_kt,
         fs_rid["del_kt"] - reg_best_kt,
         100.0 * (fs_rid["del_kt"] - reg_best_kt) / fs_rid["del_kt"],
         fs_rid["raw_kt"] - reg_best_kt,
         100.0 * (fs_rid["raw_kt"] - reg_best_kt) / fs_rid["raw_kt"]))
    W("\n### Residual band width (|residual| percentiles)\n")
    W("| predictor | median | p90 | p99 | max | mean |")
    W("|---|---:|---:|---:|---:|---:|")
    W("| bracket-NN (k=1) | %d | %d | %d | %d | %.0f |"
      % (st1[0], st1[1], st1[2], st1[3], st1[4]))
    W("| median-of-4 (k=4) | %d | %d | %d | %d | %.0f |"
      % (st4[0], st4[1], st4[2], st4[3], st4[4]))

    # ------------------------------------------------------------------
    # Part 4 -- interwiki links
    # ------------------------------------------------------------------
    W("\n## Part 4 -- interwiki links `[[xx:Title]]`\n")
    W("Language-code prefixes matching `^[a-z]{2,3}(-[a-z]+){0,2}:` "
      "(fr, de, zh-min-nan, ...; excludes long codes like `simple`). "
      "Classification is hierarchical/mutually exclusive: exact own title > "
      "casefold+strip own title > sibling repeat (same normalized title as "
      "another interwiki in the same page) > residual.\n")
    W("| metric | value |")
    W("|---|---:|")
    W("| interwiki occurrences | %d |" % iw_occ)
    W("| target-title bytes (after colon) | %d |" % iw_bytes)
    W("| mean target length (bytes) | %.2f |"
      % ((iw_bytes / iw_occ) if iw_occ else 0.0))
    W("| distinct language codes | %d |" % len(iw_langs))
    W("| == own page title exactly | %d (%.2f%%) |"
      % (iw_own_exact, bpct(iw_own_exact, iw_occ)))
    W("| == own title after casefold/strip | %d (%.2f%%) |"
      % (iw_own_key, bpct(iw_own_key, iw_occ)))
    W("| == sibling interwiki title (same page) | %d (%.2f%%) |"
      % (iw_sibling, bpct(iw_sibling, iw_occ)))
    W("| residual (matches neither) | %d (%.2f%%) |"
      % (iw_resid, bpct(iw_resid, iw_occ)))
    W("| predictable total (own or sibling) | %d (%.2f%%) |"
      % (iw_own_exact + iw_own_key + iw_sibling,
         bpct(iw_own_exact + iw_own_key + iw_sibling, iw_occ)))
    W("| (info) exact-string sibling repeats among non-own | %d |"
      % iw_sibling_exact)
    iw_top = sorted(iw_langs.items(), key=lambda kv: kv[1], reverse=True)[:15]
    W("\nTop language codes: %s\n"
      % ", ".join("%s (%d)" % (l, c) for l, c in iw_top))

    # ------------------------------------------------------------------
    # Part 5 -- Rambot census pages
    # ------------------------------------------------------------------
    W("\n## Part 5 -- Rambot census pages\n")
    W("Pages whose `<text>` contains the literal string "
      "`As of the [[census]]` (case-sensitive).\n")
    W("| metric | value |")
    W("|---|---:|")
    W("| census pages | %d (%.2f%% of pages) |"
      % (census_pages, bpct(census_pages, page_count)))
    W("| total text bytes of those pages | %d (%.2f%% of enwik9) |"
      % (census_bytes, bpct(census_bytes, total_size)))
    W("| mean text bytes per census page | %.0f |"
      % ((census_bytes / census_pages) if census_pages else 0.0))

    # read paragraph
    def hl(fs):
        return fs["raw_kt"] - fs["del_kt"]
    W("\n## Read: where is the real headroom?\n")
    W("- **Page ids** carry the strongest delta signal (mono %.2f%% positive): "
      "raw KT %.0f B collapses to %.0f B under delta-varint, a %.0f B / %.1f%% "
      "cut -- essentially a monotone counter with small gaps.\n"
      % (fs_pid["mono_pct"], fs_pid["raw_kt"], fs_pid["del_kt"], hl(fs_pid),
         bpct(hl(fs_pid), fs_pid["raw_kt"])))
    W("- **Revision ids** are near-monotone (%.2f%%) but with large jumps, so "
      "the delta-varint gives %.0f B vs %.0f B raw (%.1f%%). "
      % (fs_rid["mono_pct"], fs_rid["del_kt"], fs_rid["raw_kt"],
         bpct(hl(fs_rid), fs_rid["raw_kt"])))
    W("- **Timestamps**: best model is **%s** at %.0f B vs %.0f B raw KT "
      "(%.1f%% cut).\n"
      % (ts_best, ts_best_bytes, fs_ts["raw_kt"],
         bpct(fs_ts["raw_kt"] - ts_best_bytes, fs_ts["raw_kt"])))
    W("- **Contributor ids** are NOT monotone (%.2f%% positive) -- delta gives "
      "%.0f B vs %.0f B raw (%.1f%%); headroom here is modest and comes from "
      "the KT model itself, not the delta.\n"
      % (fs_cid["mono_pct"], fs_cid["del_kt"], fs_cid["raw_kt"],
         bpct(hl(fs_cid), fs_cid["raw_kt"])))
    total_raw = sum(fs["raw_kt"] for fs in (fs_pid, fs_rid, fs_cid))
    total_del = fs_pid["del_kt"] + fs_rid["del_kt"] + fs_cid["del_kt"]
    total_raw_ts = total_raw + fs_ts["raw_kt"]
    total_del_ts = total_del + ts_best_bytes
    W("- **Aggregate** (all four fields): KT(raw) %.0f B -> best-transform "
      "%.0f B, a floor drop of %.0f B (%.1f%%). This is the achievable "
      "structured-field entropy floor to price against the compressor's "
      "measured per-class cost.\n"
      % (total_raw_ts, total_del_ts, total_raw_ts - total_del_ts,
         bpct(total_raw_ts - total_del_ts, total_raw_ts)))

    with open(REPORT, "w", encoding="utf-8") as rf:
        rf.write("\n".join(L) + "\n")
    print("WROTE %s (%d lines)" % (REPORT, len(L)))

    # ASCII console summary (data for orchestrator)
    print("----- SUMMARY (bytes) -----")
    print("pages=%d titles=%d pageopen=%d no_contrib=%d"
          % (page_count, title_tags, page_open_tags, pages_without_contrib))
    for fs in (fs_pid, fs_rid, fs_cid, fs_ts):
        print("FIELD %-14s count=%d raw=%d KTraw=%.0f dvbytes=%d KTdelta=%.0f mono+=%.3f%%"
              % (fs["name"], fs["count"], fs["raw_bytes"], fs["raw_kt"],
                 fs["del_varint_bytes"], fs["del_kt"], fs["mono_pct"]))
    print("TS second-delta=%.0f split=%.0f best=%s(%.0f)"
          % (sec_kt, split_kt, ts_best, ts_best_bytes))
    print("LINKS occ=%d tbytes=%d distinct=%d seen_occ=%d(%.2f%%) seen_bytes=%d(%.2f%%)"
          % (link_occ, link_target_bytes, distinct_targets, seen_occ,
             bpct(seen_occ, link_occ), seen_bytes, bpct(seen_bytes, link_target_bytes)))
    print("CAT occ=%d tbytes=%d distinct=%d seen_occ=%d(%.2f%%)"
          % (cat_occ, cat_target_bytes, distinct_cats, cat_seen_occ,
             bpct(cat_seen_occ, cat_occ)))
    print("REDIR count=%d with_target=%d titlelike=%d (%.2f%% of w/target)"
          % (redirect_count, redirect_with_target, redirect_titlelike,
             bpct(redirect_titlelike, redirect_with_target)))
    print("REG pairs=%d k1: bytes=%d KT=%.0f med=%d p90=%d p99=%d max=%d"
          % (len(reg_pairs), len(stream1), kt_res1,
             st1[0], st1[1], st1[2], st1[3]))
    print("REG pairs=%d k4: bytes=%d KT=%.0f med=%d p90=%d p99=%d max=%d"
          % (len(reg_pairs), len(stream4), kt_res4,
             st4[0], st4[1], st4[2], st4[3]))
    print("REG baselines: KTdelta=%.0f KTraw=%.0f best=%s(%.0f)"
          % (fs_rid["del_kt"], fs_rid["raw_kt"], reg_best_name, reg_best_kt))
    print("IW occ=%d bytes=%d meanlen=%.2f langs=%d own_exact=%d(%.2f%%) "
          "own_key=%d(%.2f%%) sibling=%d(%.2f%%) resid=%d(%.2f%%)"
          % (iw_occ, iw_bytes, (iw_bytes / iw_occ) if iw_occ else 0.0,
             len(iw_langs),
             iw_own_exact, bpct(iw_own_exact, iw_occ),
             iw_own_key, bpct(iw_own_key, iw_occ),
             iw_sibling, bpct(iw_sibling, iw_occ),
             iw_resid, bpct(iw_resid, iw_occ)))
    print("CENSUS pages=%d bytes=%d" % (census_pages, census_bytes))
    print("DONE %.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
