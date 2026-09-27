export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
LOG=LOG_RUNMAP1G.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL -DRUNMAP -DS2RICH"
g++ -O3 -std=c++17 -fpermissive $C lpaq_x.cpp -o lpaq_run.exe 2>>$LOG || { say "build fail"; echo RUNMAP1G_DONE; exit 1; }
say "runmap+S2RICH @100MB vs 2-stage(18554027)"
./lpaq_run.exe 9 e100m.bin r.100 2>>$LOG; S=$(stat -c%s r.100)
say "  @100MB = $S  delta = $(python3 -c "print(f'{18554027-$S:+d} B = {(18554027-$S)/18554027*100:+.4f}%')")"
GO=$(python3 -c "print(1 if (18554027-$S)/18554027*100 > 0.3 else 0)")
if [ "$GO" = 1 ]; then
  say "holds @100MB. Full enwik9..."
  ./lpaq_run.exe 9 enwik9 enwik9.run 2>>$LOG; E=$(stat -c%s enwik9.run)
  python3 -c "b=154878487;p=151390679;e=$E;print(f'[RESULT] enwik9 runmap+S2RICH: {e} B = {e*8/1e9:.5f} bpc | vs orig {b-e:+d} = {(b-e)/b*100:+.3f}% | vs 2stage(151390679) {p-e:+d} B = {(p-e)/p*100:+.4f}% | XWRT=151200000 -> {(\"PASS #7\" if e<151200000 else \"still above XWRT by %d\"%(e-151200000))}')" | tee -a $LOG
  say "roundtrip..."
  ./lpaq_run.exe d enwik9.run enwik9.run.dec 2>>$LOG
  cmp -s enwik9 enwik9.run.dec && say "ROUNDTRIP LOSSLESS OK" || say "*** ROUNDTRIP FAIL"
  rm -f enwik9.run.dec
else say "did not hold @100MB"; fi
say "=== RUNMAP 1G DONE ==="; echo RUNMAP1G_DONE
