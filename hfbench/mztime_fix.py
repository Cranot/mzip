#!/usr/bin/env python3
"""Instrumentation-only: construct the timer table before the Dump so the Dump destructor
runs while the table is still alive (static destruction is reverse construction order)."""
import sys, shutil
P=sys.argv[1]; s=open(P).read()
old="inline Dump& dumper(){ static Dump d; return d; }"
new="inline Dump& dumper(){ table(); static Dump d; return d; }   // table() first: it must outlive d"
assert s.count(old)==1; s=s.replace(old,new); shutil.copy(P,P+".bak-mztime"); open(P,"w").write(s); print("PATCHED mztime")
