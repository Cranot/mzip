export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
LOG=LOG_SSE.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL"
BASE=5637995
chk(){ # name flags
  local nm="$1"; shift
  g++ -O3 -std=c++17 -fpermissive $C "$@" lpaq_x.cpp -o lpaq_$nm.exe 2>>$LOG || { say "$nm build FAIL"; return; }
  ./lpaq_$nm.exe 9 e30m.bin t.$nm 2>>$LOG; local S=$(stat -c%s t.$nm)
  say "$nm @30MB = $S  delta vs base = $(python3 -c "print(f'{$BASE-$S:+d} B = {($BASE-$S)/$BASE*100:+.4f}%')")"
  ./lpaq_$nm.exe d t.$nm t.$nm.dec 2>>$LOG
  cmp -s e30m.bin t.$nm.dec && say "  $nm lossless OK" || say "  *** $nm ROUNDTRIP FAIL"
  rm -f t.$nm.dec t.$nm
}
say "base@30MB=$BASE"
chk sse45 -DSSE45
chk s2sse -DS2RICH -DSSE45
say "=== SSE SCREEN DONE ==="; echo SSE_DONE
