#!/usr/bin/env bash
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_MORE.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
say "more: waiting for MEM test to finish (MEM DONE) + slot free..."
while ! grep -q "MEM DONE" LOG_MEM.txt 2>/dev/null; do sleep 60; done
while tasklist 2>/dev/null | grep -qiE 'lpaq_mem|lpaq_more|lpaq_final'; do sleep 30; done
C="-DFULL -DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DRUNMAP -DS2RICH -DISSECH -DMORE"
B100=18386079    # runmap+S2RICH+ISSECH @100MB
say "Building +MORE (5 models: word-suffix x2, sparse{2,3}, rolling-order, prev-prev-word)."
g++ -O3 -std=c++17 -fpermissive $C lpaq_x.cpp -o lpaq_more.exe 2>>$LOG || { say "BUILD FAIL"; echo MORE_DONE; exit 1; }
# losslessness gate (30MB)
./lpaq_more.exe 9 e30m.bin mo.30 2>>$LOG; ./lpaq_more.exe d mo.30 mo.30.dec 2>>$LOG
L=fail; cmp -s e30m.bin mo.30.dec && L=ok; rm -f mo.30 mo.30.dec
say "MORE 30MB roundtrip: $L"
# 100MB gauge (subsumption shows at scale)
./lpaq_more.exe 9 e100m.bin mo.100 2>>$LOG; M=$(stat -c%s mo.100); rm -f mo.100
say "MORE @100MB = $M  vs ISSECH-base($B100) = $(python3 -c "print(f'{$B100-$M:+d} B = {($B100-$M)/$B100*100:+.4f}%')")"
say "(if >0.1% @100MB and lossless, worth a 1GB run; else more-models subsumed -> ceiling confirmed)"
say "=== MORE DONE ==="; echo MORE_DONE
