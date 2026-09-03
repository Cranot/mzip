
### THIRD PUSH 2026-09-02 (scout workflow + v10)
HF BASELINE CORRECTED: RESULT 36 = xet-core Auto compiled verbatim (agents/hf-fairness/xetsim, lz4_flex 0.13.0).
Auto = plain LZ4 on text (1.6-2.5x), bg4 on weights, None on GGUF/parquet. The C always-bg4 baseline
(RESULT 28/34) must not be quoted for text. tbl_vs_real_xet_v91.md = table vs real Xet.
v10 (petree_v10, separate tree): accept huff0 RLE plane (h==1) -> bge-small FAST 1.480 -> 2.350, wfp32 +7.2%,
all else identical; F32 class +16.4% vs Xet (was +9.4%). RESULT 37. Fold into petree AFTER the workflow ends.
Workflow wf_60398ac6-f50 (10 scouts, 2 refuters/lever, synthesis -> INNOVATION-REPORT.md): first run died on
the API session limit; relaunched 14:58 reusing artifacts under agents/<lens>/. Early signals: RESULT 35.
