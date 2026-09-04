//! xetratio: what Xet's own code holds for a file. Real CDC chunker (xet_data::deduplication::Chunker at
//! TARGET_CHUNK_SIZE), real per-chunk scheme choice (CompressionScheme::Auto -> choose_from_data), real
//! LZ4 / BG4-LZ4 encoders. Prints bytes in, bytes out, ratio, chunk count and the scheme mix.
//! Exact-duplicate chunks within the file are counted once (Xet dedups by chunk hash).
use std::collections::HashSet;
use std::fs::File;
use std::io::Read;

use xet_core_structures::xorb_object::CompressionScheme;
use xet_data::deduplication::Chunker;
use xet_data::deduplication::constants::TARGET_CHUNK_SIZE;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: xetratio <file> [<file> ...]");
        std::process::exit(2);
    }
    println!("{:44} {:>13} {:>13} {:>7} {:>7} {:>6} {:>6} {:>6} {:>6}", "file", "bytes", "xet bytes", "ratio", "chunks", "dedup", "none", "lz4", "bg4");
    for path in &args[1..] {
        let mut f = File::open(path).expect("open");
        let mut chunker = Chunker::new(*TARGET_CHUNK_SIZE);
        let mut buf = vec![0u8; 8 * 1024 * 1024];
        let (mut n_in, mut n_out, mut n_chunks, mut n_dup) = (0u64, 0u64, 0u64, 0u64);
        let (mut c_none, mut c_lz4, mut c_bg4) = (0u64, 0u64, 0u64);
        let mut seen: HashSet<String> = HashSet::new();
        let mut handle = |chunk: xet_data::deduplication::Chunk| {
            n_in += chunk.data.len() as u64;
            n_chunks += 1;
            let h = format!("{}", chunk.hash);
            if !seen.insert(h) {
                n_dup += 1;
                return;
            }
            let scheme = CompressionScheme::choose_from_data(&chunk.data);
            let out = scheme.compress_from_slice(&chunk.data).expect("compress");
            // Xet keeps the smaller of compressed and raw per chunk
            let stored = if out.len() < chunk.data.len() { out.len() } else { chunk.data.len() };
            match scheme {
                CompressionScheme::ByteGrouping4LZ4 => c_bg4 += 1,
                CompressionScheme::LZ4 => c_lz4 += 1,
                _ => c_none += 1,
            }
            if stored == chunk.data.len() { c_none += 0; }
            n_out += stored as u64;
        };
        loop {
            let r = f.read(&mut buf).expect("read");
            if r == 0 { break; }
            for c in chunker.next_block(&buf[..r], false) { handle(c); }
        }
        if let Some(c) = chunker.finish() { handle(c); }
        let name = path.rsplit('/').next().unwrap_or(path);
        println!("{:44} {:>13} {:>13} {:>7.4} {:>7} {:>6} {:>6} {:>6} {:>6}", name, n_in, n_out, n_in as f64 / n_out.max(1) as f64, n_chunks, n_dup, c_none, c_lz4, c_bg4);
    }
}
