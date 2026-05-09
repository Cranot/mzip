# Real-World File Benchmark Summary

**47 files** from public GitHub repos (React, Linux kernel, Django, Bootstrap, and 20+ programming languages). Compares mzip against zstd:19, gzip:9, bzip2:9, xz:9, brotli:11, 7z:mx9, rar:m5.

## Headline

- **mzip wins or ties: 13 / 47 (27.7%)**
- brotli:11 wins: 32 files
- bzip2:9 wins: 1 files
- xz:9 wins: 1 files

## Where mzip wins (13 files)

| File | Size | mzip ratio | 2nd best | Advantage |
|------|-----:|-----------:|----------|-----------|
| apache_log_sample.log | 2.26MB | 22.83x | brotli:11: 115.8KB | +12.5% |
| events.csv | 578.5KB | 7.13x | bzip2:9: 82.0KB | +1.1% |
| lodash.js | 532.5KB | 7.85x | bzip2:9: 69.2KB | +2.1% |
| app.log | 464.1KB | 7.90x | bzip2:9: 60.3KB | +2.5% |
| nginx_access.log | 417.0KB | 12.11x | bzip2:9: 34.9KB | +1.3% |
| linux_kernel.c | 281.0KB | 4.41x | bzip2:9: 64.3KB | +1.0% |
| metrics.prom | 175.6KB | 10.12x | bzip2:9: 17.5KB | +1.1% |
| users.json | 169.6KB | 10.11x | bzip2:9: 17.0KB | +1.0% |
| go_http.go | 128.1KB | 3.74x | brotli:11: 34.3KB | +0.2% |
| dashboard.html | 42.5KB | 34.04x | brotli:11: 1.3KB | +7.1% |
| styles.css | 19.6KB | 9.28x | bzip2:9: 2.1KB | +1.3% |
| handlers.go | 14.0KB | 17.72x | brotli:11: 814B | +0.9% |
| docker-compose.yml | 3.9KB | 4.23x | brotli:11: 1.0KB | +8.2% |

## Where others win (34 files)

| File | Size | Winner | mzip gap |
|------|-----:|--------|---------:|
| contributing.md | 6.6KB | **brotli:11: 2.1KB** | +21.7% |
| sql_schema.sql | 4.1KB | **brotli:11: 1.1KB** | +20.0% |
| k8s_deployments.yaml | 21.1KB | **brotli:11: 1.0KB** | +18.8% |
| api_docs.md | 17.4KB | **brotli:11: 1.1KB** | +17.4% |
| ruby_rails.rb | 14.5KB | **brotli:11: 4.0KB** | +16.6% |
| services.py | 22.4KB | **brotli:11: 1.3KB** | +13.7% |
| kotlin_stdlib.kt | 20.1KB | **brotli:11: 3.8KB** | +13.6% |
| vscode_main.ts | 19.8KB | **brotli:11: 4.6KB** | +12.6% |
| models.rs | 16.8KB | **brotli:11: 826B** | +12.5% |
| xml_maven.xml | 45.4KB | **brotli:11: 5.8KB** | +12.1% |
| handlers.ts | 24.0KB | **brotli:11: 1.3KB** | +11.5% |
| json_github_api.json | 6.6KB | **brotli:11: 1.2KB** | +11.4% |
| scala_list.scala | 20.4KB | **brotli:11: 5.3KB** | +10.4% |
| rust_lib.rs | 27.5KB | **brotli:11: 7.4KB** | +9.9% |
| Dockerfile | 4.1KB | **brotli:11: 1.3KB** | +8.7% |
| webpack.config.js | 12.6KB | **brotli:11: 3.0KB** | +8.7% |
| users_dump.sql | 249.3KB | **xz:9: 15.6KB** | +8.2% |
| Makefile | 6.7KB | **brotli:11: 1.9KB** | +8.1% |
| java_arraylist.java | 64.6KB | **brotli:11: 11.2KB** | +7.6% |
| cpp_vector.hpp | 14.2KB | **brotli:11: 2.0KB** | +7.5% |
| julia_base.jl | 85.0KB | **brotli:11: 18.9KB** | +6.4% |
| elixir_genserver.ex | 49.1KB | **brotli:11: 12.0KB** | +6.0% |
| linux_makefile | 70.6KB | **brotli:11: 18.6KB** | +5.3% |
| zig_std.zig | 190.8KB | **brotli:11: 28.2KB** | +4.5% |
| bootstrap.css | 273.7KB | **brotli:11: 23.9KB** | +3.8% |
| swift_stdlib.swift | 83.3KB | **brotli:11: 17.5KB** | +3.4% |
| csharp_list.cs | 42.4KB | **brotli:11: 6.4KB** | +3.3% |
| clojure_core.clj | 269.7KB | **brotli:11: 53.6KB** | +3.2% |
| lua_neovim.lua | 52.0KB | **brotli:11: 12.5KB** | +3.1% |
| terraform_main.tf | 6.3KB | **brotli:11: 1.8KB** | +2.9% |
| flask_app.py | 62.8KB | **brotli:11: 14.5KB** | +2.0% |
| php_laravel.php | 46.1KB | **brotli:11: 7.1KB** | +1.6% |
| readme_large.md | 79.8KB | **brotli:11: 21.2KB** | +1.4% |
| django_models.py | 97.0KB | **bzip2:9: 17.6KB** | +0.2% |

## Per-category breakdown

| Category | mzip wins | Total | Win% |
|----------|-----------|-------|------|
| Logs | 3 | 3 | 100% |
| CSV / columnar | 1 | 1 | 100% |
| Metrics | 1 | 1 | 100% |
| Web | 2 | 3 | 67% |
| JSON | 1 | 2 | 50% |
| Config | 1 | 4 | 25% |
| Source code | 4 | 25 | 16% |
| Other | 0 | 2 | 0% |
| Markdown | 0 | 3 | 0% |
| SQL | 0 | 2 | 0% |
| XML | 0 | 1 | 0% |

