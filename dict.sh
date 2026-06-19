#!/usr/bin/env bash
# DICT (online word-frequency context) screen, on the run-map+S2RICH+ISSECH base. Zero decompressor cost.
# Waits for match3.sh to finish (MATCH3 DONE), then 30MB roundtrip -> 100MB. No auto-1GB (final bundle chosen by hand).
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_DICT.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL -DRUNMAP -DS2RICH -DISSECH"
B30=5586824; B100=18386079
say "dict: waiting for match3 to finish (MATCH3 DONE marker)..."
while ! grep -q "MATCH3 DONE" LOG_MATCH3.txt 2>/dev/null; do sleep 60; done
while tasklist 2>/dev/null | grep -qiE 'lpaq_m3|lpaq_dict'; do sleep 30; done
say "slot free. Building DICT (online word-freq) on run-map+S2RICH+ISSECH base."
g++ -O3 -std=c++17 -fpermissive $C -DDICT lpaq_x.cpp -o lpaq_dict.exe 2>>$LOG || { say "BUILD FAIL"; echo DICT_DONE; exit 1; }
./lpaq_dict.exe 9 e30m.bin d.30 2>>$LOG; D30=$(stat -c%s d.30)
say "DICT @30MB = $D30  vs base($B30) = $(python3 -c "print(f'{$B30-$D30:+d} B = {($B30-$D30)/$B30*100:+.4f}%')")"
./lpaq_dict.exe d d.30 d.30.dec 2>>$LOG
cmp -s e30m.bin d.30.dec && say "  DICT lossless OK" || say "  *** DICT ROUNDTRIP FAIL"
rm -f d.30.dec d.30
./lpaq_dict.exe 9 e100m.bin d.100 2>>$LOG; D100=$(stat -c%s d.100); rm -f d.100
say "DICT @100MB = $D100  vs base($B100) = $(python3 -c "print(f'{$B100-$D100:+d} B = {($B100-$D100)/$B100*100:+.4f}%')")"
say "=== DICT DONE ==="; echo DICT_DONE
