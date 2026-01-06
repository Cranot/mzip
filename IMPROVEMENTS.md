# mzip Improvement Plan (Jan 2026)

Generated from 3-perspective Opus analysis: Compression Ratio, User Adoption, Architecture.

## Current State
- **Win rate:** 74% (186/250 tests), 98% at 1MB
- **Weakness:** bzip2 wins 64 tests at 4-16KB (14-88 byte gaps)
- **Decompression:** 49x slower than zstd (BWT vs LZ77 trade-off)
- **Codebase:** 15K line monolith, 400+ line if/else cascades

---

## Phase 1: Foundation (Low effort, enables everything)

### 1A. Unit Tests for Strategies
**Why:** Enables safe refactoring, prevents regressions like "Audio PCM 2nd best was made up"

**What to test:**
- LINEAR_GEN: detection (base/delta params), roundtrip, expected compression (<50 bytes for 256KB IDs)
- TEMPLATE: roundtrip, variable extraction
- BWT_TEXT: roundtrip at various sizes
- COLUMNAR: 9-column format, roundtrip

**Framework:** googletest or catch2

### 1B. Micro Format for Small Files (4-16KB)
**Why:** +8-12% win rate, flip ~20-30 bzip2 losses to wins

**Current overhead:** 17-31 bytes
- magic (4) + version (1) + original_size (8) + block_count (4) + block_headers

**New format:** ~5 bytes
- 2-byte magic ("MZ") + 1-byte strategy + varint size

**Targets:**
| File | Current | New (est) | Saved |
|------|---------|-----------|-------|
| Natural text 4KB | 741 | ~715 | 26 |
| Markdown 4KB | 751 | ~725 | 26 |
| TOML 4KB | 1064 | ~1040 | 24 |

**Implementation:** Branch in compress() for single-block files <16KB

---

## Phase 2: User Value (Medium effort, high impact)

### 2A. Easy Integration
**Why:** "30 seconds vs 30 minutes setup" determines adoption

**Actions:**
1. Create `mzip_amalgamated.hpp` (like SQLite, stb)
2. Bundle libsais inline
3. Make zstd optional with graceful degradation
4. CMake `find_package` support
5. vcpkg/conan package submission

### 2B. Context-Aware BWT Models
**Why:** +3-5% on code files, close bzip2 gap

**Current:** Fixed English prose model (55% zeros, exponential decay)
**Problem:** Code has different MTF patterns

**New models:**
- PROSE (current default)
- CODE (higher symbol diversity)
- CONFIG (key-value patterns)
- BINARY (uniform distribution)

**Detection:** Count `{`, `}`, `:`, indentation, keywords
**Overhead:** 1 byte in header for model selection

---

## Phase 3: Architecture (Enables future growth)

### 3A. Strategy Interface + Registry
**Why:** Extensibility, testability, less duplication

**Current problem:**
```cpp
// analyze_block - 400+ lines
if (detect_linear_gen(...)) return result;
if (detect_geometric(...)) return result;
// ... 30+ more
```

**New design:**
```cpp
struct ICompressionStrategy {
    virtual BlockType type() const = 0;
    virtual int priority() const = 0;
    virtual optional<BlockAnalysis> detect(data, n) = 0;
    virtual vector<uint8_t> encode(data, n, analysis) = 0;
    virtual vector<uint8_t> decode(encoded, size, orig_size) = 0;
};
```

### 3B. Split Files
**Structure:**
```
mzip/
├── mzip.hpp              # Main API
├── core/
│   ├── block_types.hpp   # Enums, BlockAnalysis
│   ├── varint.hpp        # Encoding utilities
│   └── mdl.hpp           # MDL scoring
├── strategies/
│   ├── linear_gen.hpp
│   ├── template.hpp
│   ├── columnar.hpp
│   └── ...
└── strategy_registry.hpp
```

**Do AFTER** strategy interface is in place.

---

## Deferred (High complexity or lower priority)

### Decompression Speed
**Problem:** 49x slower than zstd - dealbreaker for read-heavy workloads
**Root cause:** BWT inverse transform O(n), MTF decode O(n*k)
**Options:**
- SIMD-accelerated MTF
- Cache-friendly inverse BWT
- Hybrid: zstd for hot paths, BWT only when ratio gain > threshold

### Streaming API
**Problem:** Current API requires full buffer, fails on large files
**Solution:** Expose MC (Multi-Chunk) through API
**Challenge:** Detection assumes full data visibility

### Config Template Detection
**Problem:** YAML/TOML/INI miss template detection
**Solution:** CONFIG_TEMPLATE strategy - extract keys, separate values, LINEAR_GEN on numerics
**Potential:** +10-15% on config files

---

## Key Insights

1. **bzip2 wins are header overhead** - 64 losses at 4-16KB with 14-88 byte gaps. Micro format fixes most.

2. **49x decompression is adoption killer** - "write-once-read-rarely" is narrow niche. Must address for mainstream adoption.

3. **Tests enable everything** - Can't safely refactor 15K monolith without tests. Do tests FIRST.

4. **BWT model mismatch** - English prose model on code files loses 30-100 bytes vs bzip2's per-file adaptation.
