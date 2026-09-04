import json
R = json.load(open("/root/mzip-hfbench/qgq/rootindex_result.json")); f = R["found"]; pb = R["pb"]
for k in ("exact", "fine-tune", "perturbed"):
    xs = [x for x in f if x["kind"] == k]; c = [x for x in xs if x["r2"] == x["r2"]]
    print(f"{k:10s} n={len(xs):2d} bytes {sum(x['bytes'] for x in xs)/1e12:.3f} TB ({100*sum(x['bytes'] for x in xs)/pb:.1f}% of probed); second-tensor confirmed n={len(c)} {sum(x['bytes'] for x in c)/1e12:.3f} TB; untagged {sum(1 for x in xs if not x['declared'])}")
ours = [x for x in f if x["kind"] in ("fine-tune", "perturbed")]
print("ours byte-weighted delta h0 %.2f b/w; count-median %.2f" % (sum(x["bytes"] * x["h"] for x in ours) / sum(x["bytes"] for x in ours), sorted(x["h"] for x in ours)[len(ours) // 2]))
print("solo pool %d repos %.2f TB; probed %d, %.2f TB" % (R["n_solo"], R["solo_b"] / 1e12, R["probed"], pb / 1e12))
print("why:", R["why"])
