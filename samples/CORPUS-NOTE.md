
## Synthetic secrets in the benchmark corpus

`samples/1m/input/env_file.env` is a generated 1 MB sample of an `.env`-shaped file used to benchmark
compression of configuration text. Every value in it (hundreds of `*_PASSWORD`, `*_KEY`, `*_SECRET`
entries) is random and corresponds to no service, account or host. It is kept byte-for-byte because
the corpus numbers depend on it. Secret scanners will flag it; there is nothing to rotate.
