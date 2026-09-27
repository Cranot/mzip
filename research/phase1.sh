#!/usr/bin/env bash
# Phase 1 cheap-win screen, on top of the confirmed 2-stage base. Self-waits for the roundtrip.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_PHASE1.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL"

say "Phase1: waiting for 2-stage roundtrip to finish (lpaq_2s)..."
while tasklist 2>/dev/null | grep -qi lpaq_2s; do sleep 30; done
if ! grep -q "ROUNDTRIP LOSSLESS OK" LOG_2STAGE.txt; then
  say "*** ABORT: 2-stage roundtrip NOT confirmed lossless. Debug before building on it."; echo PHASE1_ABORT; exit 1; fi
say "2-stage CONFIRMED lossless (151,390,679). Screening cheap wins on top."

# --- 2-stage base @30MB reference ---
./lpaq_2s.exe 9 e30m.bin p1.base 2>>$LOG; B30=$(stat -c%s p1.base)
say "2-stage @30MB base = $B30"

# --- ISSE2 (2nd linear input per context) @30MB + losslessness ---
if g++ -O3 -std=c++17 -fpermissive $C -DISSE2 lpaq_x.cpp -o lpaq_2s_isse.exe 2>>$LOG; then
  ./lpaq_2s_isse.exe 9 e30m.bin p1.isse 2>>$LOG; I30=$(stat -c%s p1.isse)
  say "ISSE2 @30MB = $I30  delta = $(python3 -c "print(f'{$B30-$I30:+d} B = {($B30-$I30)/$B30*100:+.4f}%')")"
  ./lpaq_2s_isse.exe d p1.isse p1.isse.dec 2>>$LOG
  cmp -s e30m.bin p1.isse.dec && say "  ISSE2 lossless OK" || say "  *** ISSE2 ROUNDTRIP FAIL"
  rm -f p1.isse.dec p1.isse
else say "ISSE2 build failed"; fi

# --- memory scaling HTBITS=33 (8GB hash) @100MB (needs scale to matter; may OOM) ---
if g++ -O3 -std=c++17 -fpermissive $C -DHTBITS=33 lpaq_x.cpp -o lpaq_2s_h33.exe 2>>$LOG; then
  ./lpaq_2s_h33.exe 9 e100m.bin p1.h33 2>>$LOG
  if [ -f p1.h33 ] && [ "$(stat -c%s p1.h33)" -gt 1000000 ]; then
    H=$(stat -c%s p1.h33)
    say "HTBITS=33 @100MB = $H  vs 2stage@100MB(18554027) = $(python3 -c "print(f'{18554027-$H:+d} B = {(18554027-$H)/18554027*100:+.4f}%')")"
  else say "HTBITS=33: no valid output (likely OOM at 8GB hash) — stay at 32"; fi
  rm -f p1.h33
else say "HTBITS=33 build failed"; fi

say "=== PHASE1 SCREEN DONE — analyze, combine winners, confirm at 1GB ==="
echo PHASE1_DONE
