export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
LOG=LOG_TUNE.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL"
BASE=5637995   # 2-stage base @30MB
say "S2RICH: stage-2 gated by (order,class). base@30MB=$BASE"
g++ -O3 -std=c++17 -fpermissive $C -DS2RICH lpaq_x.cpp -o lpaq_s2rich.exe 2>>$LOG
./lpaq_s2rich.exe 9 e30m.bin t.s2rich 2>>$LOG; S=$(stat -c%s t.s2rich)
say "S2RICH @30MB = $S  delta = $(python3 -c "print(f'{$BASE-$S:+d} B = {($BASE-$S)/$BASE*100:+.4f}%')")"
./lpaq_s2rich.exe d t.s2rich t.s2rich.dec 2>>$LOG
cmp -s e30m.bin t.s2rich.dec && say "  S2RICH lossless OK" || say "  *** S2RICH ROUNDTRIP FAIL"
rm -f t.s2rich.dec t.s2rich
say "=== TUNE DONE ==="; echo TUNE_DONE
