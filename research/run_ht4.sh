#!/usr/bin/env bash
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_HT4.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
while tasklist 2>/dev/null | grep -qiE 'lpaq_ht4|lpaq_mem|lpaq_more|lpaq_final'; do sleep 30; done
C="-DFULL -DHTBITS=33 -DMMIDX=29 -DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DRUNMAP -DS2RICH -DISSECH -DHT4"
say "Building best + 8GB hash + 4-way HashTable (HT4)."
g++ -O3 -std=c++17 -fpermissive $C lpaq_x.cpp -o lpaq_ht4.exe 2>>$LOG || { say "BUILD FAIL"; echo HT4_DONE; exit 1; }
# losslessness gate (core HashTable change) @30MB
./lpaq_ht4.exe 9 e30m.bin h.30 2>>$LOG; ./lpaq_ht4.exe d h.30 h.30.dec 2>>$LOG
L=fail; cmp -s e30m.bin h.30.dec && L=ok; rm -f h.30 h.30.dec
say "HT4 30MB roundtrip: $L"
if [ "$L" != ok ]; then say "*** NOT LOSSLESS — abort"; echo HT4_DONE; exit 1; fi
say "Full enwik9 (4-way collision handling; effect only at 1GB) vs best 149,596,273..."
./lpaq_ht4.exe 9 enwik9 enwik9.ht4 2>>$LOG; E=$(stat -c%s enwik9.ht4)
python3 -c "p=149596273;e=$E;print(f'[RESULT] enwik9 8GB-hash+4way: {e} B = {e*8/1e9:.5f} bpc | vs best({p}) {p-e:+d} B = {(p-e)/p*100:+.4f}%')" | tee -a $LOG
./lpaq_ht4.exe d enwik9.ht4 enwik9.ht4.dec 2>>$LOG
cmp -s enwik9 enwik9.ht4.dec && say "ROUNDTRIP LOSSLESS OK" || say "*** ROUNDTRIP FAIL"
rm -f enwik9.ht4.dec
say "=== HT4 DONE ==="; echo HT4_DONE
