# Stockfish NNUE evaluation

CCE uses `nn-134a887f4c8f.nnue` through the matching Stockfish implementation
in `stockfish/`. The C engine calls it through `cce_nnue.h` and `cce_nnue.cpp`.
A C11 compiler and a C++17 compiler are required for an NNUE build.

## Build and select evaluation

From the project root in Windows PowerShell, with 64-bit MinGW-w64 GCC/G++
and Python 3.8+.8+ installed:

```powershell
python tools\build.py
.\nnue_benchmark.exe .\nn-134a887f4c8f.nnue --check
.\cce_engine.exe --uci
```

This creates `cce_engine.exe`, `engine_diagnostics.exe`, and
`nnue_benchmark.exe`. For an AVX2-capable x86-64 CPU, build with
`python tools\build.py --simd avx2`. Do not use that binary on a CPU without AVX2.
An explicit `--simd scalar` build has no AVX2 requirement. The default build detects native SIMD support.

On Linux:

```sh
make -j4 all diagnostics nnue-test
./nnue_benchmark ./nn-134a887f4c8f.nnue --check
./cce_engine --uci
```

Alternatively use `python3 tools/build.py`; it also supports Clang via
`CC=clang CXX=clang++`. In Termux, install `clang` and `python`, then use that
Python build command. For ARM64 NEON, add `--simd neon`.

The network is an external file, not embedded in the executable. Put it in
the engine's working directory, or set its absolute path before starting:

```powershell
$env:CCE_NNUE_FILE = 'D:\Chess-Engine\nn-134a887f4c8f.nnue'
.\cce_engine.exe --play
```

A valid network enables NNUE by default in UCI, terminal play, and puzzle
play. If it cannot be loaded, CCE reports the reason on stderr and uses
PeSTO. To select PeSTO from the start, set `$env:CCE_EVAL = 'pesto'`; remove
that override with `Remove-Item Env:CCE_EVAL`. Linux uses
`CCE_EVAL=pesto ./cce_engine`. A C-only build is available through
`python tools\build.py --pesto-only` or `make NNUE=0 all diagnostics`.
Do not compile only `src/*.c` and expect NNUE: the C++ module must be linked.

Inside a UCI GUI, choose `EvalFile` and `UseNNUE`. Equivalent UCI commands are:

```text
setoption name EvalFile value D:\Chess-Engine\nn-134a887f4c8f.nnue
setoption name UseNNUE value true
```

`UseNNUE=false` selects PeSTO. Successful backend changes clear the
transposition table. A failed file reload preserves the previous network.
If NNUE was disabled when `EvalFile` was changed, enable `UseNNUE` afterward.

## Supported network and arithmetic

This integration accepts only the target network, identified by:

- File size: 98,961,994 bytes.
- SHA-256: `134a887f4c8ff7bf7284177a3b3fc6ff9cef95ba89eb8db3079a8e507f7126af`.
- Container version: `0x6a448afa`; architecture hash: `0xa85b2205`.
- Features: HalfKAv2_hm piece-square/king features, FullThreats, and PP_3Wide
  pawn pairs; 86,896 feature dimensions in total.
- Quantized 1024-wide transformer, 32-unit dense layers, squared and linear
  clipped activations, skip output, eight layer stacks and PSQT buckets.

Loading checks the entire SHA-256 before decoding, then verifies upstream
feature and layer hashes, parameters, and end-of-file. Castling, en passant,
captures, promotions, king moves, and null moves update upstream dirty-feature
records and incremental accumulators. Undo restores their matching state.
Directly replaced boards are refreshed. Paths beyond the upstream accumulator
capacity use a refresh rather than writing past its stack.

Raw NNUE scores use Stockfish internal units. CCE converts these to
centipawns with the pinned upstream material-dependent conversion. Positive
scores favor the side to move; puzzle output converts them to White's
perspective. Stockfish's search optimism and rule50 evaluation damping are
not applied; CCE retains its own draw handling and search. These centipawns
are an evaluation scale, not an exact material count or a promised win.
The NNUE state is single-threaded, matching CCE's current search.

## Validation and comparison

```powershell
.\nnue_benchmark.exe .\nn-134a887f4c8f.nnue --check
.\nnue_benchmark.exe .\nn-134a887f4c8f.nnue
python tests\nnue_load_test.py .\nnue_benchmark.exe .\nn-134a887f4c8f.nnue
python tests\nnue_uci_test.py .\cce_engine.exe .\nn-134a887f4c8f.nnue
.\engine_diagnostics.exe --stress --depth 7
```

`--check` compares both perspectives' accumulator and PSQT values and final
raw scores against cold full refreshes, including special moves, random game
paths, null moves, undo, and deep-stack recovery. The default benchmark runs
those checks, then compares PeSTO and NNUE at depth 6 and at 1500 CPU ms on
four positions, clearing the TT between runs. It reports completed depth,
score, best move, nodes, quiescence nodes, CPU seconds, and NPS. `--bench`
skips correctness checks for timing only. Results alone do not establish Elo.

To compare raw outputs with an independently built Stockfish from the pinned
revision, run:

```powershell
python tests\nnue_oracle.py .\nnue_benchmark.exe .\stockfish.exe .\nn-134a887f4c8f.nnue
```

Use the exact revision in `stockfish/UPSTREAM.md`. This test checks the raw
internal score, avoiding rounding differences in the displayed centipawns.
The supplied vendor source can also build a separate Stockfish oracle with
its own Makefile; use `EXTRACXXFLAGS=-DNNUE_EMBEDDING_OFF` and configure its
`EvalFile` to the supplied network. The oracle is a test dependency; CCE does
not launch Stockfish to evaluate during play.

For address/undefined-behavior checks on a supported GCC/Clang host:
`python tools/build.py --sanitize --output-dir build/asan`, then run the same
checks against the binaries in that directory.

## Header-only inspection utility

`network_check.c` remains a separate container-header checker. It does not
perform inference. Its message about inference applies to that utility,
not the engine's C++ loader.

```powershell
cc -O2 -std=c11 nnue/network_file.c nnue/network_check.c -o nnue_check.exe
.\nnue_check.exe .\nn-134a887f4c8f.nnue
(Get-FileHash .\nn-134a887f4c8f.nnue -Algorithm SHA256).Hash
```

On Linux use `./nnue_check` and `sha256sum nn-134a887f4c8f.nnue`.
Letter case in the hash does not matter.

## Upstream license

The vendored Stockfish source is GPLv3-or-later. Preserve `stockfish/AUTHORS`,
`stockfish/Copying.txt`, and its source notices when redistributing. The
combined NNUE executable must be distributed under compatible GPLv3 terms
with corresponding source. See `stockfish/UPSTREAM.md` for provenance.

## Native acceleration and profiling

`python tools/build.py` detects native AVX2 or NEON support; the previous
scalar default was slow for this network. `--simd scalar` remains available
for portability. An AVX2 build is intended for CPUs supporting AVX2; compile
for the destination CPU or explicitly choose the portable mode when sharing.
Android ARM64 builds automatically use NEON. Make uses `SIMD=auto` by default.
The UCI handshake reports the compiled NNUE compute backend.

For diagnostics, add `--nnue-profile` (Make: `NNUE_PROFILE=1`). The benchmark
reports four accumulator perspective paths: cached, incremental, refresh
from the existing king-square cache, and hybrid king-move update. A refresh
path is not a cold reload of all network parameters. Counts include two
perspectives per live evaluation. Cold validation calls are outside measured
search snapshots. Production builds omit this diagnostic accounting.

Search time limits use elapsed wall time with a 64-node time-check interval.
UCI `time`, `nps`, and `pv` fields make GUI speed reporting available.
A 5s + 0.05s control normally grants only about 191ms for an early move under
the current budget formula; it does not grant five seconds per move.
