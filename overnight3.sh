#!/usr/bin/env bash
# Chained AFTER overnight2.sh. Per-context isolation of the wiki pack on the STRONG coder @100MB.
# Each run = XPN=2 + exactly ONE wiki context -> clean marginal vs XPN=2 (18,839,513).
# Logs to OVERNIGHT_LOG3.txt.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=OVERNIGHT_LOG3.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }

say "overnight3: waiting for overnight2 to complete..."
while ! grep -q "OVERNIGHT2 COMPLETE" OVERNIGHT_LOG2.txt 2>/dev/null; do sleep 60; done
while tasklist 2>/dev/null | grep -qiE 'lpaq_full_idnum|lpaq_XPN2_WIKI|lpaq_wikifull|lpaq_n0full|lpaq_n2full|lpaq_one'; do sleep 30; done

XPN2=18839513
say "overnight3: per-context isolation @100MB. Each = XPN=2 + ONE wiki ctx. ref XPN2=$XPN2"
names="richmode_x_word container_x_word container_x_lastbyte list container_x_class richmode_x_class container_x_richmode"
i=0
for nm in $names; do
  if g++ -O3 -std=c++17 -fpermissive -DFULL -DXPN=2 -DWIKI -DWIKIONE=$i lpaq_iso.cpp -o lpaq_one$i.exe 2>>$LOG; then
    ./lpaq_one$i.exe 9 e100m.bin e100.one$i 2>>$LOG
    SZ=$(stat -c%s e100.one$i)
    say "  ctx$i $nm : $SZ  marginal vs xpn2 = $(python3 -c "print(f'{$XPN2-$SZ:+d} B = {($XPN2-$SZ)/$XPN2*100:+.4f}%')")"
    rm -f lpaq_one$i.exe e100.one$i
  else
    say "  ctx$i $nm : BUILD FAILED"
  fi
  i=$((i+1))
done
say "Positive marginals = contexts worth keeping; <=0 = subsumed/harmful (drop). Build pruned pack in the morning."
say "=== OVERNIGHT3 COMPLETE ==="
echo OVERNIGHT3_DONE
