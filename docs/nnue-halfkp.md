# HalfKP NNUE

The default evaluator uses Stockfish 12's frozen network with Latrunculi's own
scalar and AVX2 inference kernels. The network, score adapter, and AUTO backend
match the untuned candidate tested in OpenBench #105. NNUE is the only evaluator.

## Build

On Linux with GCC or Clang:

```bash
cmake --preset release-dev
cmake --build --preset release-dev --parallel 2
ctest --preset release-dev
```

CMake copies `networks/nn-82215d0fd0df.nnue` into the build directory. Ship it
beside `latrunculi`. The engine resolves its own executable through Linux's
`/proc/self/exe` and reads that sibling file once, independent of the launcher's
working directory or a symlink used to start the engine. Weights remain immutable
and shared across workers. Missing, truncated, incompatible, or modified files
fail initialization explicitly; there is no evaluator fallback or download.

The fixed network SHA256 is
`82215d0fd0df3cde17fc47ac024c5b5736823ec390200abdd705c44660c69ee4`.
Loading, format validation, SHA256, and immutable weight ownership are kept in
`nnue_network`; accumulator updates, inference, and score adaptation are in
`nnue`. Engine startup calls `nnue::initialize()` before accepting commands.
The loader validates the format and arithmetic bounds before use. There are
no evaluator, network-path, or backend CMake options. `EvalFile` and network
switching are deferred; replacing the fixed file with other weights is unsupported.

Automatic backend selection checks CPU support once. AVX2 functions have their
own compiler target; generic code retains its normal CPU requirements. The
portable scalar implementation remains the reference in tests and the offline
verifier. Builds need neither Python nor network access, and no assembler blob
is generated. Native verification tools are built with `BUILD_TESTING=ON`.

`make -C bench` uses the same defaults for OpenBench workers and copies both
files to its output directory. The current OpenBench client subsequently moves
only the executable into its engine cache, so workers also need the pinned
network provisioned beside those cached executables before a campaign. Worker
builds use two jobs. The separate tuning entry point still enables SPSA search
options; HCE fitting and feature export have been removed.

The `eval` console command reports the network, backend, raw inference score,
and adapted static score. UCI options and search parameters are unchanged.

## Arithmetic and state contract

The legacy feature map excludes kings as pieces, indexes other pieces relative
to each king, and rotates squares by `^63` for Black. Latrunculi's color enum is
translated explicitly. Transformer values are clipped to `[0,127]`; the hidden
affine layers shift by six bits and clip to the same range. The final numerator
is divided by 16 with signed truncation toward zero.

Raw inference must exactly match `Eval::NNUE::compute_eval()` from the pinned
Stockfish revision. The separate, frozen score adapter computes
`neural = trunc(raw*5/4)+28`, then `trunc(neural*202/208)`, clamped to `±30000`.
The ratio bridges the engines' endgame pawn units. This is an initial integration
policy, not fitted calibration or proof of compatibility with existing pruning.
No HCE blending, additional tempo, or Stockfish rule-50 damping is applied.
Latrunculi retains its own rule and draw handling.

Each reversible history entry owns both aligned accumulators. Moves copy their
parent's accumulators and apply feature deltas; king moves refresh the affected
perspective after placement is complete. Unmake restores the parent snapshot.
Null moves preserve the accumulators and change the inference ordering. Network
weights are immutable and shared; board histories are independent across workers.

## Offline verification

The optional Python tools use the project's existing `python-chess` environment.
`python -m tools.nnue.verify prepare` freezes 10,000 legal histories with seed
98170, adds special cases, and records outputs from a separately built reference
oracle. It refuses to overwrite an existing corpus. The oracle patch only adds
`nnuepush` and `nnueraw` diagnostic commands; its inference arithmetic is unchanged.

To recreate the oracle in an empty cache directory:

```bash
git clone --depth 1 --branch sf_12 https://github.com/official-stockfish/Stockfish.git \
  .cache/stockfish-halfkp-oracle
test "$(git -C .cache/stockfish-halfkp-oracle rev-parse HEAD)" = \
  c306d838697011da0a960758dde3f7ede6849060
git -C .cache/stockfish-halfkp-oracle apply "$PWD/tools/nnue/oracle.patch"
cp networks/nn-82215d0fd0df.nnue .cache/stockfish-halfkp-oracle/src/
make -C .cache/stockfish-halfkp-oracle/src -j2 build ARCH=general-64 COMP=gcc
python -m tools.nnue.verify prepare --output OUTPUT \
  --oracle .cache/stockfish-halfkp-oracle/src/stockfish \
  --network networks/nn-82215d0fd0df.nnue
```

Use the existing project virtual environment for the Python commands. `OUTPUT`
must be a new corpus destination; keep its identity records and oracle transcripts.

```bash
python -m tools.nnue.verify check --output OUTPUT \
  --executable build/release-dev/latrunculi-neural-verify \
  --label auto-parity --all-backends
```

The native verifier accepts `FEN<TAB>optional UCI history` per line and emits JSON
with raw/adapted scores and network/backend identities. `0000` represents a null
move outside check. Every transition and unmake checks incremental versus fresh
state; final positions also compare scalar and AVX2 intermediate activations.
Release, sanitizer, and controlled runtime evidence belongs in `tools/output/`.
