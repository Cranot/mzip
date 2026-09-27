#!/usr/bin/env bash
# Phase 2b/3 screen: MATCH2 (delta recovery) and ISSECH (ISSE chaining) on the run-map base.
# Self-waits for the run-map 1GB confirmation (lpaq_run) to free the slot. 30MB + roundtrip each.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_PHASE23.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL -DRUNMAP -DS2RICH"

say "phase23: waiting for run-map 1GB (lpaq_run) to finish..."
while tasklist 2>/dev/null | grep -qi lpaq_run; do sleep 30; done
say "slot free. base = run-map + S2RICH."

REF=0
chk(){ local nm="$1"; shift
  g++ -O3 -std=c++17 -fpermissive $C "$@" lpaq_x.cpp -o lpaq_$nm.exe 2>>$LOG || { say "$nm build FAIL"; return; }
  ./lpaq_$nm.exe 9 e30m.bin t.$nm 2>>$LOG; local S=$(stat -c%s t.$nm)
  if [ "$nm" = ref ]; then REF=$S; say "ref(runmap+S2RICH) @30MB = $S";
  else say "$nm @30MB = $S  delta vs ref = $(python3 -c "print(f'{$REF-$S:+d} B = {($REF-$S)/$REF*100:+.4f}%')")"; fi
  ./lpaq_$nm.exe d t.$nm t.$nm.dec 2>>$LOG
  cmp -s e30m.bin t.$nm.dec && say "  $nm lossless OK" || say "  *** $nm ROUNDTRIP FAIL ***"
  rm -f t.$nm.dec t.$nm
}
chk ref
chk match2 -DMATCH2
chk issech -DISSECH
chk both   -DMATCH2 -DISSECH
say "=== PHASE23 SCREEN DONE ==="; echo PHASE23_DONE
