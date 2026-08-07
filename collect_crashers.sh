#!/usr/bin/env bash
# Drive fuzz_decode, collecting EVERY distinct crasher into fuzz_corpus/ by resuming past each crash.
seed=${1:-0xD00D}; N=${2:-3000000}; start=0; crashes=0
mkdir -p fuzz_corpus
while [ $start -lt $N ]; do
  ./fuzz_decode.exe $N $seed $start > /dev/null 2>&1
  rc=$?
  if [ $rc -eq 0 ]; then echo "CLEAN: survived to $N (crashes found=$crashes)"; break; fi
  citer=$(grep -o 'iter=[0-9]*' fuzzd_cur.txt | head -1 | cut -d= -f2)
  csize=$(grep -o 'size=[0-9]*' fuzzd_cur.txt | head -1 | cut -d= -f2)
  if [ -z "$citer" ]; then echo "crash but no iter (rc=$rc) at start=$start"; break; fi
  cp fuzzd_cur.bin "fuzz_corpus/c_${citer}_${csize}.bin"
  echo "CRASH iter=$citer size=$csize rc=$rc -> fuzz_corpus/c_${citer}_${csize}.bin"
  crashes=$((crashes+1))
  start=$((citer+1))
  if [ $crashes -ge 40 ]; then echo "stopping at 40 crashers"; break; fi
done
echo "TOTAL distinct crashers this pass: $crashes"
ls fuzz_corpus/c_*.bin 2>/dev/null | wc -l
