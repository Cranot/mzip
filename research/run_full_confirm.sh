export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress
echo "=== build FULL XPN=0 and XPN=2 ==="
g++ -O3 -std=c++17 -fpermissive -DFULL -DXPN=0 lpaq_x.cpp -o lpaq_n0full.exe 2>/dev/null && echo "n0full built"
g++ -O3 -std=c++17 -fpermissive -DFULL -DXPN=2 lpaq_x.cpp -o lpaq_n2full.exe 2>/dev/null && echo "n2full built"
echo "=== 100MB matched (e100m.bin), full tables ==="
./lpaq_n0full.exe 9 e100m.bin e100.n0 2>&1
./lpaq_n2full.exe 9 e100m.bin e100.n2 2>&1
echo "e100 n0=$(stat -c%s e100.n0)  n2=$(stat -c%s e100.n2)"
echo "=== FULL enwik9 (1GB) XPN=2 vs known baseline 154,878,487 ==="
./lpaq_n2full.exe 9 enwik9 enwik9.n2 2>&1
echo "enwik9 XPN=2 = $(stat -c%s enwik9.n2)   baseline(enwik9.final) = $(stat -c%s enwik9.final)"
echo ALLDONE_FULL_CONFIRM
