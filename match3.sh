#!/usr/bin/env bash
# Full match cascade (MATCH3) screen, on top of the run-map+S2RICH+ISSECH bundle.
# Self-waits for the ISSECH 1GB run (lpaq_issech) to free the slot. 30MB roundtrip (recovery = lossless-critical)
# -> 100MB -> 1GB if it holds. The big lever toward zpaq.
export PATH="/d/Safe/Tools/w64devkit/bin:$PATH"
cd /d/Safe/Projects/tieredcompress 2>/dev/null
LOG=LOG_MATCH3.txt; : > $LOG
say(){ echo "[$(date '+%H:%M:%S')] $*" | tee -a $LOG; }
C="-DXPN=2 -DWIKI -DWIKIN=6 -DIDNUM -DMIXER_2STAGE -DFULL -DRUNMAP -DS2RICH -DISSECH"
BASE30=5586824        # runmap+S2RICH+ISSECH @30MB (from phase23)
BASE100=18386079      # runmap+S2RICH+ISSECH @100MB (from LOG_ISSECH1G)
PREV7=150305046       # current confirmed #7

say "match3: waiting for ISSECH 1GB (lpaq_issech) to finish..."
while tasklist 2>/dev/null | grep -qi lpaq_issech; do sleep 30; done
say "slot free. Building full cascade (MATCH3) on the run-map+S2RICH+ISSECH bundle."
if ! g++ -O3 -std=c++17 -fpermissive $C -DMATCH3 lpaq_x.cpp -o lpaq_m3.exe 2>>$LOG; then say "BUILD FAIL"; echo MATCH3_DONE; exit 1; fi

# --- 30MB screen + roundtrip (losslessness is the gate) ---
./lpaq_m3.exe 9 e30m.bin m3.30 2>>$LOG; M30=$(stat -c%s m3.30)
say "MATCH3 @30MB = $M30  vs base($BASE30) = $(python3 -c "print(f'{$BASE30-$M30:+d} B = {($BASE30-$M30)/$BASE30*100:+.4f}%')")"
./lpaq_m3.exe d m3.30 m3.30.dec 2>>$LOG
LOSS=fail; cmp -s e30m.bin m3.30.dec && LOSS=ok; rm -f m3.30.dec m3.30
say "  MATCH3 30MB roundtrip: $LOSS"
GO=$(python3 -c "print(1 if ($BASE30-$M30)/$BASE30*100 > 0.2 else 0)")
if [ "$LOSS" != ok ]; then say "*** NOT LOSSLESS — stop, debug cascade."; echo MATCH3_DONE; exit 1; fi
if [ "$GO" != 1 ]; then say "MATCH3 did not beat base by >0.2% @30MB (cascade may need tuning). Stopping before 1GB."; echo MATCH3_DONE; exit 0; fi

# --- 100MB ---
say "wins @30MB. 100MB vs ISSECH-base($BASE100)..."
./lpaq_m3.exe 9 e100m.bin m3.100 2>>$LOG; M100=$(stat -c%s m3.100); rm -f m3.100
say "MATCH3 @100MB = $M100  vs base = $(python3 -c "print(f'{$BASE100-$M100:+d} B = {($BASE100-$M100)/$BASE100*100:+.4f}%')")"
GO2=$(python3 -c "print(1 if ($BASE100-$M100)/$BASE100*100 > 0.15 else 0)")
if [ "$GO2" != 1 ]; then say "didn't hold @100MB. Stopping before 1GB."; echo MATCH3_DONE; exit 0; fi

# --- full enwik9 + roundtrip ---
say "holds @100MB. Full enwik9 (the big-lever result)..."
./lpaq_m3.exe 9 enwik9 enwik9.m3 2>>$LOG; E=$(stat -c%s enwik9.m3)
python3 -c "b=154878487;p=$PREV7;e=$E;print(f'[RESULT] enwik9 +MATCH3 cascade: {e} B = {e*8/1e9:.5f} bpc | vs orig {(b-e)/b*100:+.3f}% | vs #7({p}) {p-e:+d} B = {(p-e)/p*100:+.4f}% | zpaq=142300000 gap {e-142300000} B')" | tee -a $LOG
./lpaq_m3.exe d enwik9.m3 enwik9.m3.dec 2>>$LOG
cmp -s enwik9 enwik9.m3.dec && say "ROUNDTRIP LOSSLESS OK" || say "*** ROUNDTRIP FAIL"
rm -f enwik9.m3.dec
say "=== MATCH3 DONE ==="; echo MATCH3_DONE
