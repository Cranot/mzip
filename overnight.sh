#!/usr/bin/env bash
# Overnight autonomous DCLM pipeline. Self-sequences; respects RAM (one 6GB coder at a time).
# Logs to OVERNIGHT_LOG.txt. Safe to read in the morning.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=OVERNIGHT_LOG.txt
: > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
run(){ # run dclm_rig, append csv line to log
  ./dclm_rig6.exe "$1" "$2" "$3" 2>/dev/null | tee -a $LOG >/dev/null
}

say "=== OVERNIGHT DCLM PIPELINE START ==="

############### PART A: rig battery (fast, small RAM) ###############
say "PART A: rig ceiling battery (enwik6 + emid10m). base=ref, random=capacity control."
echo "--- module,ctrl,file,bytes,bpc,models ---" >> $LOG
for F in _enwik_1m.bin emid10m.bin; do
  for M in base xprod rwiki wikifull mega; do
    ./dclm_rig6.exe $F $M real   2>/dev/null >> $LOG
    [ "$M" != base ] && ./dclm_rig6.exe $F $M random 2>/dev/null >> $LOG
  done
done
say "PART A done."

############### PART B: wait for the running 1GB job (b1bfhcbif) to free RAM ###############
say "PART B: waiting for lpaq_n0full/lpaq_n2full (b1bfhcbif: roundtrip + baseline reproduce) to finish..."
while tasklist 2>/dev/null | grep -qiE 'lpaq_n0full|lpaq_n2full'; do sleep 30; done
say "PART B: RAM free. (b1bfhcbif outputs:)"
[ -f enwik9.n0 ] && say "  XPN=0 enwik9 reproduction = $(stat -c%s enwik9.n0)  (expect 154878487)"

############### PART C: strong-coder ablations at 100MB matched (full tables) ###############
# Known full-table 100MB baselines (from bioczmtwo): XPN=0=18970186, XPN=2=18839513
B0=18970186; B2=18839513
say "PART C: strong-coder WIKI ablations @100MB (e100m). baseline19=$B0  xpn2=$B2"
declare -A R100
for spec in "XPN2_WIKI:-DXPN=2 -DWIKI" "XPN2_WIKI2:-DXPN=2 -DWIKI -DWIKIN=2" "XPN2_WIKI4:-DXPN=2 -DWIKI -DWIKIN=4"; do
  name=${spec%%:*}; flags=${spec#*:}
  say "  build $name ($flags)"
  if g++ -O3 -std=c++17 -fpermissive -DFULL $flags lpaq_x.cpp -o lpaq_$name.exe 2>>$LOG; then
    ./lpaq_$name.exe 9 e100m.bin e100.$name 2>>$LOG
    sz=$(stat -c%s e100.$name); R100[$name]=$sz
    pct=$(python3 -c "print(f'{($B0-$sz)/$B0*100:+.3f}%')")
    pct2=$(python3 -c "print(f'{($B2-$sz)/$B2*100:+.3f}%')")
    say "  $name @100MB = $sz   vs baseline19 $pct   vs xpn2 $pct2"
  else
    say "  $name BUILD FAILED"
  fi
done

############### PART D: full enwik9 for the best config + roundtrip ###############
BEST=lpaq_XPN2_WIKI.exe
say "PART D: full enwik9 with best pack ($BEST) vs known baseline 154878487"
if [ -f $BEST ]; then
  ./$BEST 9 enwik9 enwik9.wiki 2>>$LOG
  WS=$(stat -c%s enwik9.wiki)
  say "  enwik9 XPN2+WIKI = $WS"
  python3 -c "b=154878487; w=$WS; print(f'[RESULT] enwik9: {w} B = {w*8/1e9:.5f} bpc  (baseline 154878487 = 1.23903)  delta {b-w:+d} B = {(b-w)/b*100:+.3f}%')" | tee -a $LOG
  say "PART D: roundtrip-verify enwik9.wiki ..."
  ./$BEST d enwik9.wiki enwik9.wiki.dec 2>>$LOG
  if cmp -s enwik9 enwik9.wiki.dec; then say "  ROUNDTRIP LOSSLESS OK"; else say "  *** ROUNDTRIP FAIL ***"; fi
  rm -f enwik9.wiki.dec
else
  say "  best build missing; skipping enwik9"
fi

say "=== OVERNIGHT PIPELINE COMPLETE ==="
echo "OVERNIGHT_PIPELINE_DONE"
