# Frozen HalfKP Network

`nn-82215d0fd0df.nnue` is the original default network of Stockfish 12.

- Architecture: legacy HalfKP, 41,024 inputs, 256 neurons per perspective,
  concatenated into affine layers of 32, 32, and 1 outputs.
- Reference source: Stockfish commit `c306d838697011da0a960758dde3f7ede6849060`.
- Source: https://tests.stockfishchess.org/api/nn/nn-82215d0fd0df.nnue
- SHA256: `82215d0fd0df3cde17fc47ac024c5b5736823ec390200abdd705c44660c69ee4`.
- Size: 21,022,697 bytes.

The Stockfish network repository distributes its networks under CC0; the upstream
legal text is retained in [LICENSE](LICENSE). See
https://github.com/official-stockfish/networks for provenance. Latrunculi's kernels
are its own implementation of the documented format and architecture. The
reference oracle is built separately from the pinned GPLv3 Stockfish source.

Builds embed this exact file and never download it automatically. Other network
files are unsupported in this bootstrap milestone.
