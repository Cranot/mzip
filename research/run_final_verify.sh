export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
echo "=== (1) 1GB roundtrip: decompress XPN=2 output, must equal enwik9 ==="
./lpaq_n2full.exe d enwik9.n2 enwik9.n2.dec 2>&1
if cmp -s enwik9 enwik9.n2.dec; then echo "ROUNDTRIP_LOSSLESS_OK"; else echo "ROUNDTRIP_FAIL"; fi
rm -f enwik9.n2.dec
echo "=== (2) reproduce XPN=0 baseline on enwik9 (must equal 154,878,487) ==="
./lpaq_n0full.exe 9 enwik9 enwik9.n0 2>&1
echo "XPN=0 enwik9 = $(stat -c%s enwik9.n0)   recorded baseline = 154878487"
echo ALLDONE_FINAL_VERIFY
