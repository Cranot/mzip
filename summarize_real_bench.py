#!/usr/bin/env python3
"""Summarize real_bench_results.txt into a markdown report.

Parses each per-file block and emits:
- Win/loss counts per compressor
- Where mzip wins (with ratios)
- Where mzip loses (with gap %)
- Per-category breakdown if extension is recognizable

Run: python summarize_real_bench.py real_bench_results.txt > real_bench_summary.md
"""
import re
import sys
from collections import defaultdict

EXT_CATEGORY = {
    ('.log',): 'Logs',
    ('.csv',): 'CSV / columnar',
    ('.json',): 'JSON',
    ('.html', '.css'): 'Web',
    ('.md',): 'Markdown',
    ('.yml', '.yaml', '.toml', '.ini', '.tf', 'erfile'): 'Config',
    ('.js', '.ts', '.py', '.go', '.rs', '.c', '.cpp', '.hpp', '.h', '.java', '.kt', '.rb',
     '.php', '.scala', '.clj', '.ex', '.jl', '.lua', '.swift', '.zig', '.cs', '.sh'): 'Source code',
    ('.xml',): 'XML',
    ('.sql',): 'SQL',
    ('.prom',): 'Metrics',
}

def categorize(name):
    low = name.lower()
    for keys, cat in EXT_CATEGORY.items():
        for k in keys:
            if low.endswith(k):
                return cat
    return 'Other'

def main():
    if len(sys.argv) < 2:
        print("Usage: summarize_real_bench.py <results.txt>", file=sys.stderr)
        sys.exit(1)
    with open(sys.argv[1]) as f:
        data = f.read()

    blocks = re.split(r'={70,}\s*\n\s*FILE BENCHMARK: ', data)[1:]
    files = []
    for b in blocks:
        m = re.match(r'(\S+) \((\d+) bytes\)', b)
        if not m:
            continue
        name = m.group(1)
        size = int(m.group(2))
        # Stop parsing at end of per-file table (the summary line "Original:")
        per_file = b.split('Original:', 1)[0]
        sizes = {}
        for cm in re.finditer(r'^(mzip|zstd:19|gzip:9|bzip2:9|xz:9|brotli:11|7z:mx9|rar:m5)\s+(\d+)', per_file, re.M):
            if cm.group(1) not in sizes:
                sizes[cm.group(1)] = int(cm.group(2))
        winm = re.search(r'Best:\s+(\S+) \((\d+) bytes', b)
        if winm:
            winner = winm.group(1)
            winner_size = int(winm.group(2))
        else:
            continue
        if 'mzip' not in sizes:
            continue
        mzip_size = sizes['mzip']
        files.append({
            'name': name, 'size': size, 'sizes': sizes,
            'winner': winner, 'winner_size': winner_size,
            'mzip_size': mzip_size,
            'mzip_won': mzip_size <= winner_size,
        })

    total = len(files)
    mzip_wins = sum(1 for f in files if f['mzip_won'])
    by_winner = defaultdict(int)
    for f in files:
        if f['mzip_won']:
            by_winner['mzip'] += 1
        else:
            by_winner[f['winner']] += 1

    print(f"# Real-World File Benchmark Summary")
    print()
    print(f"**{total} files** from public GitHub repos (React, Linux kernel, Django, Bootstrap, "
          f"and 20+ programming languages). Compares mzip against zstd:19, gzip:9, bzip2:9, "
          f"xz:9, brotli:11, 7z:mx9, rar:m5.")
    print()
    print(f"## Headline")
    print()
    print(f"- **mzip wins or ties: {mzip_wins} / {total} ({100.0*mzip_wins/total:.1f}%)**")
    for k, v in sorted(by_winner.items(), key=lambda x: -x[1]):
        if k != 'mzip':
            print(f"- {k} wins: {v} files")
    print()

    # Where mzip wins
    print(f"## Where mzip wins ({mzip_wins} files)")
    print()
    print("| File | Size | mzip ratio | 2nd best | Advantage |")
    print("|------|-----:|-----------:|----------|-----------|")
    wins = sorted([f for f in files if f['mzip_won']], key=lambda f: -f['size'])
    for f in wins:
        ratio = f['size'] / max(f['mzip_size'], 1)
        # find 2nd best
        others = {k: v for k, v in f['sizes'].items() if k != 'mzip'}
        if not others:
            continue
        second_name = min(others, key=others.get)
        second_size = others[second_name]
        adv = (second_size - f['mzip_size']) / second_size * 100.0
        print(f"| {f['name']} | {format_size(f['size'])} | {ratio:.2f}x | "
              f"{second_name}: {format_size(second_size)} | "
              f"{'+' if adv >= 0 else ''}{adv:.1f}% |")
    print()

    # Where others win
    losses = [f for f in files if not f['mzip_won']]
    print(f"## Where others win ({len(losses)} files)")
    print()
    print("| File | Size | Winner | mzip gap |")
    print("|------|-----:|--------|---------:|")
    losses.sort(key=lambda f: (f['mzip_size'] - f['winner_size']) / f['winner_size'], reverse=True)
    for f in losses:
        gap_pct = (f['mzip_size'] - f['winner_size']) / f['winner_size'] * 100.0
        print(f"| {f['name']} | {format_size(f['size'])} | "
              f"**{f['winner']}: {format_size(f['winner_size'])}** | +{gap_pct:.1f}% |")
    print()

    # Category breakdown
    cat_counts = defaultdict(lambda: [0, 0])  # [wins, total]
    for f in files:
        cat = categorize(f['name'])
        cat_counts[cat][1] += 1
        if f['mzip_won']:
            cat_counts[cat][0] += 1
    print(f"## Per-category breakdown")
    print()
    print("| Category | mzip wins | Total | Win% |")
    print("|----------|-----------|-------|------|")
    for cat, (w, t) in sorted(cat_counts.items(), key=lambda x: -x[1][0] / max(x[1][1], 1)):
        pct = 100.0 * w / max(t, 1)
        print(f"| {cat} | {w} | {t} | {pct:.0f}% |")
    print()

def format_size(b):
    if b < 1024:
        return f"{b}B"
    if b < 1024 * 1024:
        return f"{b/1024:.1f}KB"
    return f"{b/(1024*1024):.2f}MB"

if __name__ == "__main__":
    main()
