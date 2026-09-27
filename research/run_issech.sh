export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
LOG=LOG_ISSECH1G.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL -DRUNMAP -DS2RICH -DISSECH"
g++ -O3 -std=c++17 -fpermissive $C lpaq_x.cpp -o lpaq_issech.exe 2>>$LOG || { say "build fail"; echo ISSECH1G_DONE; exit 1; }
say "runmap+S2RICH+ISSECH @100MB vs runmap-base(18450884)"
./lpaq_issech.exe 9 e100m.bin i.100 2>>$LOG; S=$(stat -c%s i.100)
say "  @100MB = $S  delta = $(python3 -c "print(f'{18450884-$S:+d} B = {(18450884-$S)/18450884*100:+.4f}%')")"
GO=$(python3 -c "print(1 if (18450884-$S)/18450884*100 > 0.15 else 0)")
if [ "$GO" = 1 ]; then
  say "holds @100MB. Full enwik9..."
  ./lpaq_issech.exe 9 enwik9 enwik9.issech 2>>$LOG; E=$(stat -c%s enwik9.issech)
  python3 -c "b=154878487;p=150305046;e=$E;print(f'[RESULT] enwik9 +ISSECH: {e} B = {e*8/1e9:.5f} bpc | vs orig {(b-e)/b*100:+.3f}% | vs #7(150305046) {p-e:+d} B = {(p-e)/p*100:+.4f}%')" | tee -a $LOG
  say "roundtrip..."
  ./lpaq_issech.exe d enwik9.issech enwik9.issech.dec 2>>$LOG
  cmp -s enwik9 enwik9.issech.dec && say "ROUNDTRIP LOSSLESS OK" || say "*** ROUNDTRIP FAIL"
  rm -f enwik9.issech.dec
else say "did not hold @100MB"; fi
say "=== ISSECH 1G DONE ==="; echo ISSECH1G_DONE
