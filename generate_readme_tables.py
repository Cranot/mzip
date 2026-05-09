#!/usr/bin/env python3
"""
Generate README markdown tables from benchmark CSV output.
Run: python generate_readme_tables.py full_bench.csv
"""

import csv
import sys
from collections import defaultdict

# Type name -> sample filename mappings
TYPE_TO_FILE = {
    'Database IDs': ('db_ids.bin', 'bin'),
    'Timestamps (regular)': ('timestamps_regular.bin', 'bin'),
    'Timestamps (jitter)': ('timestamps_jitter.bin', 'bin'),
    'Integer array': ('int_array.bin', 'bin'),
    'GPS coordinates': ('gps_track.bin', 'bin'),
    'Float temperature': ('floats.bin', 'bin'),
    'Sensor 16-bit': ('sensor_16bit.bin', 'bin'),
    'Audio PCM': ('audio_pcm.bin', 'bin'),
    'Image gradient': ('gradient.bin', 'bin'),
    'Executable': ('executable.bin', 'bin'),
    'Sparse bitmap': ('sparse_bitmap.bin', 'bin'),
    'Protobuf-like': ('protobuf.bin', 'bin'),
    'Random bytes': ('random.bin', 'bin'),
    'JSON API': ('json.json', 'json'),
    'JSON structured log': ('json_log.json', 'json'),
    'XML document': ('xml.xml', 'xml'),
    'CSV data': ('csv.csv', 'csv'),
    'GraphQL queries': ('graphql.graphql', 'graphql'),
    'SQL dump': ('sql.sql', 'sql'),
    'Base64 data': ('base64.txt', 'txt'),
    'JavaScript': ('javascript.js', 'js'),
    'Python': ('python.py', 'py'),
    'TypeScript/JSX': ('typescript.ts', 'ts'),
    'HTML': ('html.html', 'html'),
    'CSS': ('css.css', 'css'),
    'Go': ('golang.go', 'go'),
    'Rust': ('rust.rs', 'rs'),
    'Java': ('java.java', 'java'),
    'C': ('c.c', 'c'),
    'Bash script': ('bash.sh', 'sh'),
    'PHP': ('php.php', 'php'),
    'Docker Compose': ('docker_compose.yaml', 'yaml'),
    'K8s manifests': ('k8s.yaml', 'yaml'),
    'Terraform HCL': ('terraform.tf', 'tf'),
    'YAML config': ('yaml_config.yaml', 'yaml'),
    'TOML config': ('toml_config.toml', 'toml'),
    'INI config': ('ini_config.ini', 'ini'),
    '.env file': ('dotenv.env', 'env'),
    'package.json': ('package.json', 'json'),
    'Cargo.toml': ('cargo.toml', 'toml'),
    'Makefile': ('makefile', 'makefile'),
    'Access log': ('access_log.log', 'log'),
    'Nginx access log': ('nginx_access.log', 'log'),
    'Syslog': ('syslog.log', 'log'),
    'Metrics/TSeries': ('metrics.txt', 'txt'),
    'Email headers': ('email_headers.txt', 'txt'),
    'Natural text': ('natural_text.txt', 'txt'),
    'Unicode text': ('unicode.txt', 'txt'),
    'Markdown docs': ('markdown.md', 'md'),
}

# Categories matching README structure
CATEGORIES = {
    'NUMERIC': ['Timestamps (regular)', 'Database IDs', 'Integer array',
                'GPS coordinates', 'Float temperature', 'Sensor 16-bit'],
    'STRUCTURED': ['GraphQL queries', 'SQL dump', 'JSON API', 'XML document',
                   'CSV data', 'Base64 data'],
    'CODE': ['JavaScript', 'Python', 'TypeScript/JSX', 'HTML', 'CSS', 'Go',
             'Rust', 'Java', 'C', 'Bash script', 'PHP'],
    'CONFIG': ['Docker Compose', 'Terraform HCL', 'K8s manifests', 'YAML config'],
    'LOG': ['Access log', 'Nginx access log'],
    'BINARY': ['Image gradient', 'Audio PCM', 'Sparse bitmap', 'Protobuf-like'],
    'ADDITIONAL': ['Natural text', 'Markdown docs', 'Email headers', 'Unicode text',
                   'Syslog', 'Metrics/TSeries', 'JSON structured log', 'Timestamps (jitter)'],
    'BUILD': ['Makefile', 'package.json', 'Cargo.toml'],
}

# Display names (shorter for README)
DISPLAY_NAMES = {
    'Timestamps (regular)': 'Timestamps',
    'TypeScript/JSX': 'TypeScript',
    'Terraform HCL': 'Terraform',
    'Bash script': 'Bash',
    'JSON structured log': 'JSON log',
    'Timestamps (jitter)': 'Timestamps (jitter)',
    'Metrics/TSeries': 'Metrics',
}

def format_size(b):
    """Format bytes to human readable (32B, 1.7KB, etc.)"""
    if b < 1024:
        return f"{b}B"
    elif b < 10240:
        return f"{b/1024:.1f}KB"
    else:
        return f"{int(b/1024)}KB"

def get_sample_links(t, sizes=['64k', '256k', '1m']):
    """Generate sample file links for a type"""
    if t not in TYPE_TO_FILE:
        return ""
    filename, _ = TYPE_TO_FILE[t]
    links = []
    for sz in sizes:
        links.append(f"[{sz}](samples/{sz}/input/{filename})")
    return " ".join(links)

def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_readme_tables.py full_bench.csv")
        sys.exit(1)

    # Read CSV
    rows = []
    with open(sys.argv[1], 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    # Group by type and size
    by_type = defaultdict(dict)
    for row in rows:
        t = row['type']
        size = row['size']
        by_type[t][size] = row

    sizes = ['64KB', '256KB', '1MB']
    competitors = ['zstd', 'gzip', 'bzip2', 'xz', 'brotli', '7z', 'rar']

    # Generate tables for each category
    for cat, types in CATEGORIES.items():
        print(f"\n### {cat}\n")
        print("| Type | 64KB | 256KB | 1MB | Samples |")
        print("|------|------|-------|-----|---------|")

        for t in types:
            if t not in by_type:
                continue

            display_name = DISPLAY_NAMES.get(t, t)
            cols = [f"**{display_name}**"]

            for sz in sizes:
                if sz not in by_type[t]:
                    cols.append("-")
                    continue

                row = by_type[t][sz]
                mzip = int(row['mzip'])

                # Find best non-mzip
                best_other = float('inf')
                for comp in competitors:
                    if comp in row and row[comp]:
                        try:
                            val = int(row[comp])
                            if val > 0:
                                best_other = min(best_other, val)
                        except ValueError:
                            pass

                if best_other == float('inf'):
                    best_other = mzip

                mzip_s = format_size(mzip)
                other_s = format_size(best_other)

                if mzip <= best_other:
                    cols.append(f"**{mzip_s}** vs {other_s}")
                else:
                    cols.append(f"{mzip_s} vs **{other_s}**")

            # Add sample links
            cols.append(get_sample_links(t))

            print("| " + " | ".join(cols) + " |")

        print("\n*Format: **mzip** vs 2nd-best. Bold = winner.*")

    # ---------- Headline tables ----------
    competitors_full = ['mzip', 'zstd', 'gzip', 'bzip2', 'xz', 'brotli', '7z', 'rar']
    sizes_all = ['4KB', '16KB', '64KB', '256KB', '1MB']

    # Scoreboard: per-compressor totals across all (type, size) cells
    totals = {c: {'orig_sum': 0, 'comp_sum': 0, 'wins': 0, 'best_ratio': 0.0} for c in competitors_full}
    n_cells = 0
    win_by_size = {sz: {'mzip': 0, 'total': 0} for sz in sizes_all}

    for t, by_size in by_type.items():
        for sz, row in by_size.items():
            try:
                orig = int(row['original'])
            except (KeyError, ValueError):
                continue
            n_cells += 1
            if sz in win_by_size:
                win_by_size[sz]['total'] += 1

            # collect sizes for each compressor
            cell = {}
            for c in competitors_full:
                if c in row and row[c]:
                    try:
                        v = int(row[c])
                        if v > 0:
                            cell[c] = v
                    except ValueError:
                        pass
            if not cell:
                continue
            best_size = min(cell.values())
            for c, v in cell.items():
                totals[c]['orig_sum'] += orig
                totals[c]['comp_sum'] += v
                if v > 0:
                    totals[c]['best_ratio'] = max(totals[c]['best_ratio'], orig / v)
                if v == best_size:
                    totals[c]['wins'] += 1
            if 'mzip' in cell and cell['mzip'] == best_size and sz in win_by_size:
                win_by_size[sz]['mzip'] += 1

    print("\n### Overall Compressor Scoreboard\n")
    print(f"({n_cells} cells: {len(by_type)} types x {len(sizes_all)} sizes)\n")
    print("| Compressor | Avg Ratio | Best Ratio | Wins | Win% |")
    print("|------------|-----------|------------|------|------|")
    for c in competitors_full:
        t = totals[c]
        if t['comp_sum'] == 0:
            continue
        avg_ratio = t['orig_sum'] / t['comp_sum']
        winpct = 100.0 * t['wins'] / max(n_cells, 1)
        bold = "**" if c == 'mzip' else ""
        print(f"| {bold}{c}{bold} | {bold}{avg_ratio:.2f}x{bold} | {t['best_ratio']:.0f}x | {bold}{t['wins']}{bold} | {bold}{winpct:.1f}%{bold} |")

    print("\n### Win Rate by Size\n")
    print("| Size | Wins | Total | Win% |")
    print("|------|------|-------|------|")
    for sz in sizes_all:
        w = win_by_size[sz]
        if w['total'] == 0:
            continue
        pct = 100.0 * w['mzip'] / w['total']
        print(f"| {sz} | {w['mzip']} | {w['total']} | {pct:.1f}% |")

    # Top-10 mzip wins (largest mzip-vs-2nd-best advantage)
    advantages = []  # (advantage_x, type, size, mzip_size, second)
    gaps = []        # (gap_bytes, type, size, mzip_size, winner_name, winner_size)
    for t, by_size in by_type.items():
        for sz, row in by_size.items():
            try:
                mzip = int(row['mzip'])
            except (KeyError, ValueError):
                continue
            if mzip <= 0:
                continue
            others = {}
            for c in competitors_full:
                if c == 'mzip':
                    continue
                if c in row and row[c]:
                    try:
                        v = int(row[c])
                        if v > 0:
                            others[c] = v
                    except ValueError:
                        pass
            if not others:
                continue
            best_other_name = min(others, key=others.get)
            best_other = others[best_other_name]
            if mzip <= best_other:
                # mzip win or tie
                if mzip > 0:
                    advantages.append((best_other / max(mzip, 1), t, sz, mzip, best_other))
            else:
                gaps.append((mzip - best_other, t, sz, mzip, best_other_name, best_other))

    advantages.sort(reverse=True)
    gaps.sort(reverse=True)

    print("\n### Top 10 mzip Wins\n")
    print("| Type | Size | mzip | 2nd Best | Advantage |")
    print("|------|------|------|----------|-----------|")
    for adv, t, sz, mzip, other in advantages[:10]:
        print(f"| {DISPLAY_NAMES.get(t, t)} | {sz} | **{format_size(mzip)}** | {format_size(other)} | {adv:.1f}x |")

    print("\n### Where Others Win (Top 10 Gaps)\n")
    print("| Type | Size | mzip | Best | Gap |")
    print("|------|------|------|------|-----|")
    for gap, t, sz, mzip, winner, winner_size in gaps[:10]:
        print(f"| {DISPLAY_NAMES.get(t, t)} | {sz} | {format_size(mzip)} | **{winner}: {format_size(winner_size)}** | +{gap}B |")

if __name__ == "__main__":
    main()
