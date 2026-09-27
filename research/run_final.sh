#!/usr/bin/env bash
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_FINAL.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
while tasklist 2>/dev/null | grep -qiE 'lpaq_m3|lpaq_dict|lpaq_issech|lpaq_run|lpaq_final'; do sleep 30; done
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL -DRUNMAP -DS2RICH -DISSECH -DMATCH3"
M3ALONE30=5580124   # MATCH3 (no dict) @30MB, from match3.sh
say "Screen MATCH3+DICT @30MB (DICT is online/zero-cost; 30MB likely understates it)."
g++ -O3 -std=c++17 -fpermissive $C -DDICT lpaq_x.cpp -o lpaq_final.exe 2>>$LOG || { say "BUILD FAIL"; echo FINAL_DONE; exit 1; }
./lpaq_final.exe 9 e30m.bin f.30 2>>$LOG; F30=$(stat -c%s f.30)
./lpaq_final.exe d f.30 f.30.dec 2>>$LOG; L=fail; cmp -s e30m.bin f.30.dec && L=ok; rm -f f.30 f.30.dec
say "MATCH3+DICT @30MB = $F30  vs MATCH3-alone($M3ALONE30) = $(python3 -c "print(f'{$M3ALONE30-$F30:+d} B = {($M3ALONE30-$F30)/$M3ALONE30*100:+.4f}%')")  lossless=$L"
if [ "$L" != ok ]; then say "*** NOT LOSSLESS — abort"; echo FINAL_DONE; exit 1; fi
# include DICT in the 1GB if it doesn't clearly hurt @30MB (it's online -> usually helps more at scale)
if python3 -c "exit(0 if $F30 <= $M3ALONE30+1500 else 1)"; then
  say "Using MATCH3+DICT for 1GB (DICT neutral-or-better @30MB; zero decompressor cost)."
else
  say "DICT hurt @30MB; using MATCH3-only for 1GB."
  g++ -O3 -std=c++17 -fpermissive $C lpaq_x.cpp -o lpaq_final.exe 2>>$LOG
fi
say "Full enwik9 (the new smallest-file attempt) vs current #7 149,838,898..."
./lpaq_final.exe 9 enwik9 enwik9.final2 2>>$LOG; E=$(stat -c%s enwik9.final2)
python3 -c "b=154878487;p=149838898;e=$E;print(f'[RESULT] enwik9 = {e} B = {e*8/1e9:.5f} bpc | vs orig {(b-e)/b*100:+.3f}% | vs #7({p}) {p-e:+d} B = {(p-e)/p*100:+.4f}% | zpaq142.3M gap {e-142300000} B')" | tee -a $LOG
say "roundtrip..."
./lpaq_final.exe d enwik9.final2 enwik9.final2.dec 2>>$LOG
cmp -s enwik9 enwik9.final2.dec && say "ROUNDTRIP LOSSLESS OK" || say "*** ROUNDTRIP FAIL"
rm -f enwik9.final2.dec
say "=== FINAL DONE ==="; echo FINAL_DONE
