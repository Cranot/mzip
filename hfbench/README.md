# hfbench — mzip measured against Hugging Face's Xet storage on real Hub data

Everything here measures one question: on the bytes people actually upload to Hugging Face, what does
mzip save over what Xet stores today, and at what cost in time?

**Start with `MZIP-AT-HF-GRANULARITY.txt`.** It is the measurement record: every numbered RESULT, in
order, with the script that produced it, the exact figures, and what was predicted before the run.
Every number quoted anywhere else traces to a RESULT there. `LEARNINGS.md` holds the transferable
lessons — the thesis by elimination, the ideas that died, and the method traps that cost real time.

## What was found

- **Per-chunk statistical coding of quantised weights is finished.** Order-1 over K-quant codes gains
  0.00%; unpacking int32-packed 4-bit weights gains 0.3%. A good quantiser spends its codebook.
- **The largest classes of the week cannot be compressed.** Archives (380 TB/wk) are 83% already-
  compressed media inside transparent tar/zip containers. Optimizer state (144 TB/wk) has real
  structure that Xet's byte-grouped LZ4 already harvests.
- **A quantised model is the output of a program the Hub already hosts the input to.** Given its
  declared parent, a Q8_0 costs 1.4% of itself (codes and scales both recompute); Q5_K and Q6_K
  reproduce every code exactly and keep only their scales; fused MoE experts reproduce at 100%.
- **80.5% of GGUF bytes declare that parent** in Hub metadata. The recipe route needs no tensor-name map: 94.3% of those bytes have a parent that exists, ships weights and is a converter-registered architecture; a one-request norm check finds 94.4% of those are the right model; 91.3% of those convert (true rate ~97%, the rest are limits of the check). The rest is bases with no weights, architectures newer than the converter, and parents that were merges mislabelled as quantisations.
- **Net: 1,444 TB held today → 1,124 TB** for the sampled week, 22.2% less, every factor measured; 1,077 TB (25.4%) adding undeclared twins stored against their siblings.

## Layout

| path | what |
|---|---|
| `MZIP-AT-HF-GRANULARITY.txt` | the record, 115 numbered RESULTs |
| `LEARNINGS.md` | transferable lessons and method traps |
| `upload_mix.py`, `upload_mix_report.py`, `before_after.py`, `final_savings.py`, `finalnum*.py` | the week's census and the headline table |
| `kquant_given.py`, `alltypes*.py`, `crossrepo*.py`, `kgb.py`, `moe.py`, `scalefree.py` | quantisation-given-parent: the arithmetic and its measurement |
| `namemap.py`, `resolverate*.py`, `namedump.py` | GGUF → safetensors tensor-name mapping and how often a parent resolves |
| `finetune.py`, `ftfix.py` | fine-tunes given their base |
| `archprobe.py`, `tarprobe*.py`, `optimprobe.py`, `otherprobe.py`, `packed.py`, `widths.py` | the classes that turned out to be dead ends, and why |
| `upload-mix/` | the byte-weighted repo sample and its tag census (JSON) |
| `qgq/` | per-run result JSON referenced by RESULT entries |

## Reproducing

Scripts read the Hub over HTTP range requests and never download a model; set `HF_TOKEN` for gated
repos. Paths inside the scripts assume they run from this directory's parent on a Linux box with
Python 3 and numpy; adjust `B = ...` at the top of each. The fetched model bytes and sweep outputs
(≈14 GB) are deliberately not in the repo — only the text is.

## Method notes worth knowing before trusting a number

Every prediction here was written down before its run and scored after. The record marks each claim
[M] measured, [I] inferred, or [A] asserted, and carries the weakest tag in its support chain. The
instrument was wrong many times and each failure is recorded beside the corrected result — a weight
by sampled bytes instead of file size, a selector whose winning branch was a tautology, a test that
never ran and printed a blank read as a refutation. Read the RESULT before the number.
