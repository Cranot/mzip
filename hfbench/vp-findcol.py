import pyarrow.parquet as pq
orig = open("/root/mzip-hfbench/corpora/code-python.bin","rb").read()
head = orig[:4096]
pf = pq.ParquetFile("/root/mzip-hfbench/assets/code-python.parquet")
for col in ["original_string","code","whole_func_string","summary"]:
    if col not in pf.schema_arrow.names: continue
    out = bytearray()
    for batch in pf.iter_batches(batch_size=512, columns=[col]):
        for v in batch.column(0).to_pylist():
            if v is None: continue
            out += str(v).encode("utf-8","replace") + b"\n"
        if len(out) >= 8192: break
    print(col, "MATCH" if bytes(out[:4096]) == head else "no", repr(bytes(out[:60])))
