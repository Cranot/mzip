import sys, io
p = "/root/mzip-hfbench/vfy-xz/src/mzip.hpp"
src = io.open(p, "r", encoding="utf-8", errors="surrogateescape").read()
lines = src.split("\n")
# locate the marker comment line
idx = [i for i,l in enumerate(lines) if "xz (liblzma -9 EXTREME) trial" in l]
assert len(idx) == 1, ("marker count", len(idx))
start = idx[0]
assert lines[start+1].strip() == "{", repr(lines[start+1])
# brace-match from the opening brace line
depth = 0
end = None
for i in range(start+1, len(lines)):
    depth += lines[i].count("{") - lines[i].count("}")
    if depth == 0:
        end = i
        break
assert end is not None
blk = "\n".join(lines[start:end+1])
assert "lzma_easy_buffer_encode" in blk, "wrong block"
assert "BlockType::XZLIB" in blk
assert "lzma_stream_buffer_encode" not in blk, "swallowed the BCJ trial too"
sys.stderr.write("DELETING lines %d..%d (%d lines)\n" % (start+1, end+1, end-start+1))
sys.stderr.write(blk + "\n----\n")
out = lines[:start] + ["            // [VERIFIER] plain xz-9e backstop trial DELETED for A/B"] + lines[end+1:]
io.open(p, "w", encoding="utf-8", errors="surrogateescape").write("\n".join(out))
sys.stderr.write("OK patched\n")
