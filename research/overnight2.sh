#!/usr/bin/env bash
# Chained AFTER overnight.sh. Tests the full kitchen-sink (XPN=2 + WIKI + IDNUM) at scale.
# Serialized on the heavy RAM slot. Logs to OVERNIGHT_LOG2.txt.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=OVERNIGHT_LOG2.txt
: > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }

say "overnight2: waiting for overnight.sh to complete..."
while ! grep -q "OVERNIGHT PIPELINE COMPLETE" OVERNIGHT_LOG.txt 2>/dev/null; do sleep 60; done
while tasklist 2>/dev/null | grep -qiE 'lpaq_XPN2_WIKI|lpaq_wikifull|lpaq_n0full|lpaq_n2full|lpaq_full_idnum'; do sleep 30; done
say "overnight2: RAM free. Building XPN=2 + WIKI + IDNUM (full kitchen sink)."
if ! g++ -O3 -std=c++17 -fpermissive -DFULL -DXPN=2 -DWIKI -DIDNUM lpaq_x.cpp -o lpaq_full_idnum.exe 2>>$LOG; then
  say "BUILD FAILED"; echo OVERNIGHT2_DONE; exit 1
fi
say "built lpaq_full_idnum.exe"

B0=18970186   # XPN=0 full @100MB (from bioczmtwo)
./lpaq_full_idnum.exe 9 e100m.bin e100.idnum 2>>$LOG
SZ=$(stat -c%s e100.idnum)
say "XPN2+WIKI+IDNUM @100MB = $SZ   vs baseline19 $B0 = $(python3 -c "print(f'{($B0-$SZ)/$B0*100:+.3f}%')")"
WIKISZ=$(stat -c%s e100.XPN2_WIKI 2>/dev/null || echo 0)
if [ "$WIKISZ" != 0 ]; then say "  vs wiki-pack @100MB ($WIKISZ) = $(python3 -c "print(f'{($WIKISZ-$SZ)/$WIKISZ*100:+.3f}%')")"; fi

say "full enwik9 with XPN2+WIKI+IDNUM ..."
./lpaq_full_idnum.exe 9 enwik9 enwik9.idnum 2>>$LOG
ES=$(stat -c%s enwik9.idnum)
python3 -c "b=154878487;e=$ES;print(f'[RESULT] enwik9 XPN2+WIKI+IDNUM: {e} B = {e*8/1e9:.5f} bpc  delta {b-e:+d} B = {(b-e)/b*100:+.3f}%  (baseline 154878487)')" | tee -a $LOG
say "roundtrip verify ..."
./lpaq_full_idnum.exe d enwik9.idnum enwik9.idnum.dec 2>>$LOG
if cmp -s enwik9 enwik9.idnum.dec; then say "ROUNDTRIP LOSSLESS OK"; else say "*** ROUNDTRIP FAIL ***"; fi
rm -f enwik9.idnum.dec
say "=== OVERNIGHT2 COMPLETE ==="
echo OVERNIGHT2_DONE
