#!/usr/bin/env bash
# Capstone: pruned-optimal pack = XPN=2 + WIKI(WIKIN=6, drops the harmful 'list' ctx) + IDNUM.
# Compares to current best (full-7+idnum = 153,096,426). Logs to OVERNIGHT_LOG4.txt.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=OVERNIGHT_LOG4.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
while tasklist 2>/dev/null | grep -qiE 'lpaq_.*full|lpaq_one|lpaq_pruned|lpaq_n[02]'; do sleep 30; done

say "capstone: build XPN=2 + WIKI(6, no list) + IDNUM"
if ! g++ -O3 -std=c++17 -fpermissive -DFULL -DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM lpaq_x.cpp -o lpaq_pruned.exe 2>>$LOG; then
  say "BUILD FAILED"; echo OVERNIGHT4_DONE; exit 1; fi
say "built lpaq_pruned.exe"

./lpaq_pruned.exe 9 e100m.bin e100.pruned 2>>$LOG
P100=$(stat -c%s e100.pruned)
say "pruned @100MB = $P100  (full-7+idnum was 18731746: $(python3 -c "print(f'{(18731746-$P100):+d} B')"))"

say "full enwik9 ..."
./lpaq_pruned.exe 9 enwik9 enwik9.pruned 2>>$LOG
PE=$(stat -c%s enwik9.pruned)
python3 -c "b=154878487;c=153096426;p=$PE;print(f'[RESULT] enwik9 PRUNED (xpn2+wiki6+idnum): {p} B = {p*8/1e9:.5f} bpc  vs baseline {b-p:+d} = {(b-p)/b*100:+.3f}%  | vs prev-best(153096426) {c-p:+d} B')" | tee -a $LOG

say "roundtrip verify ..."
./lpaq_pruned.exe d enwik9.pruned enwik9.pruned.dec 2>>$LOG
if cmp -s enwik9 enwik9.pruned.dec; then say "ROUNDTRIP LOSSLESS OK"; else say "*** ROUNDTRIP FAIL ***"; fi
rm -f enwik9.pruned.dec
say "=== OVERNIGHT4 (CAPSTONE) COMPLETE ==="
echo OVERNIGHT4_DONE
