#!/usr/bin/env bash
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_MEM.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
say "mem: waiting for slot (combined roundtrip etc.)..."
while tasklist 2>/dev/null | grep -qiE 'lpaq_final|lpaq_m3|lpaq_issech|lpaq_run|lpaq_mem'; do sleep 30; done
# best config + 8GB hash (HTBITS 32->33) + bigger match index (MMIDX 28->29) — the scale-lever untested at 1GB
C="-DFULL -DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DRUNMAP -DS2RICH -DISSECH -DHTBITS=33 -DMMIDX=29"
say "Building best + HTBITS=33 (8GB hash) + MMIDX=29..."
if ! g++ -O3 -std=c++17 -fpermissive $C lpaq_x.cpp -o lpaq_mem.exe 2>>$LOG; then say "BUILD FAIL"; echo MEM_DONE; exit 1; fi
say "Full enwik9 with 8GB hash (vs #7 149,838,898; may OOM at ~10GB RAM)..."
./lpaq_mem.exe 9 enwik9 enwik9.mem 2>>$LOG
if [ -f enwik9.mem ] && [ "$(stat -c%s enwik9.mem)" -gt 1000000 ]; then
  E=$(stat -c%s enwik9.mem)
  python3 -c "p=149838898;e=$E;print(f'[RESULT] enwik9 8GB-hash: {e} B = {e*8/1e9:.5f} bpc | vs #7({p}) {p-e:+d} B = {(p-e)/p*100:+.4f}%')" | tee -a $LOG
  ./lpaq_mem.exe d enwik9.mem enwik9.mem.dec 2>>$LOG
  cmp -s enwik9 enwik9.mem.dec && say "ROUNDTRIP LOSSLESS OK" || say "*** ROUNDTRIP FAIL"
  rm -f enwik9.mem.dec
else say "no valid output — likely OOM at 8GB hash; stay at HTBITS=32"; fi
say "=== MEM DONE ==="; echo MEM_DONE
