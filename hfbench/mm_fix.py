#!/usr/bin/env python3
"""v10.2 (petree_v10 mzpe): Miller-Madow bias correction in plane_bits -- identical expression to the
plane-entropy package so both stay byte-identical (the package is the source of truth)."""
import sys
P = sys.argv[1]; s = open(P).read()
old = '''        double hb = 0.0;
        for (unsigned c = 0; c < 256; c++) if (h[c]) { double p = (double)h[c] / (double)L; hb -= p * std::log2(p); }
        bits += std::min(8.0, hb + 0.02) * (double)L;   // raw plane = 8 bits/byte; small table cost'''
new = '''        double hb = 0.0; unsigned K = 0;
        for (unsigned c = 0; c < 256; c++) if (h[c]) { K++; double p = (double)h[c] / (double)L; hb -= p * std::log2(p); }
        if (K > 1) hb += (double)(K - 1) / (2.0 * (double)L * 0.6931471805599453);   // v10.2: Miller-Madow (== plane-entropy package)
        bits += std::min(8.0, hb + 0.02) * (double)L;   // raw plane = 8 bits/byte; small table cost'''
assert s.count(old) == 1, "anchor count=%d" % s.count(old)
open(P, "w").write(s.replace(old, new)); print("PATCHED v10.2", P)
