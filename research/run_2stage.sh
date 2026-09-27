export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
LOG=LOG_2STAGE.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
say "2-stage @100MB: baseline (this config) vs 2-stage"
./lpaq_ref2.exe 9 e100m.bin e100.base 2>>$LOG
./lpaq_2s.exe   9 e100m.bin e100.2s   2>>$LOG
B=$(stat -c%s e100.base); S=$(stat -c%s e100.2s)
say "baseline=$B  2stage=$S  delta=$(python3 -c "print(f'{B-S:+d} B = {(B-S)/B*100:+.4f}%'.replace('B-S',str($B-$S)))" 2>/dev/null || python3 -c "print(f'{$B-$S:+d} B = {($B-$S)/$B*100:+.4f}%')")"
GO=$(python3 -c "print(1 if ($B-$S)/$B*100 > 0.4 else 0)")
if [ "$GO" = 1 ]; then
  say "100MB win confirmed (>0.4%). Running full enwik9 2-stage..."
  ./lpaq_2s.exe 9 enwik9 enwik9.2s 2>>$LOG
  E=$(stat -c%s enwik9.2s)
  python3 -c "b=154878487;p=153062900;e=$E;print(f'[RESULT] enwik9 2-STAGE: {e} B = {e*8/1e9:.5f} bpc | vs baseline {b-e:+d} = {(b-e)/b*100:+.3f}% | vs prev-best(153062900) {p-e:+d} B = {(p-e)/p*100:+.4f}%')" | tee -a $LOG
  say "roundtrip verify..."
  ./lpaq_2s.exe d enwik9.2s enwik9.2s.dec 2>>$LOG
  if cmp -s enwik9 enwik9.2s.dec; then say "ROUNDTRIP LOSSLESS OK"; else say "*** ROUNDTRIP FAIL ***"; fi
  rm -f enwik9.2s.dec
else
  say "100MB win below threshold; NOT running enwik9. Will tune selectors first."
fi
say "=== 2STAGE PIPELINE DONE ==="
echo DONE_2STAGE
