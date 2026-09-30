# CCE Chess Engine

A single-threaded chess engine with bitboard move generation, iterative-deepening search, UCI, terminal play, and a FEN puzzle solver. Its NNUE module uses the matching Stockfish feature transformer, layers, and incremental accumulators for `nn-134a887f4c8f.nnue`. PeSTO remains available as a selectable evaluator. Run commands from the project root.

## Requirements and build

Use a 64-bit C11 and C++17 toolchain (GCC/G++ or Clang/Clang++) and Python 3.8+. On Windows PowerShell with MinGW-w64 GCC/G++ in PATH:

```powershell
python tools\build.py
```

This creates `cce_engine.exe`, `engine_diagnostics.exe`, and `nnue_benchmark.exe`. For an AVX2-capable x86-64 CPU, add `--simd avx2`; otherwise retain the portable scalar default. Close running engine processes before rebuilding on Windows.

On Linux, use `make -j4 all diagnostics nnue-test` or `python3 tools/build.py`. On Termux, install `clang` and `python`, then run `CC=clang CXX=clang++ python tools/build.py --simd neon` on ARM64. C sources include project headers relative to their own directories. Keep matching versions of `src`, `inc`, `nnue`, `tools`, and `tests` together.

Put `nn-134a887f4c8f.nnue` in the working directory or set `CCE_NNUE_FILE` to its absolute path. The full loader verifies the SHA-256 and network structure before enabling NNUE. If loading fails, it reports the reason on stderr and falls back to PeSTO. To choose PeSTO at startup, set `CCE_EVAL=pesto`. See [NNUE setup and validation](nnue/README.md) for environment commands, format specifications, and tests.

A C-only build is available with `python tools\build.py --pesto-only` or `make NNUE=0 all diagnostics`. A direct `gcc src/*.c` build uses PeSTO; NNUE requires compiling and linking the C++ module.

## Run as a UCI engine

Launch `cce_engine.exe` **without arguments** in a UCI GUI such as Cute Chess, or use `--uci`. Set protocol to UCI and working directory to the project root if your GUI allows it. This is a console engine, so a GUI supplies the board. To inspect the protocol yourself:

```powershell
.\cce_engine.exe --uci
```

Once it starts, enter these lines into its standard input (not at the PowerShell prompt):

```text
uci
isready
position startpos moves e2e4 e7e5
go depth 6
quit
```

Supported commands include `position startpos [moves ...]`, `position fen ...`, `go depth N`, `go movetime MS`, clock-based `go wtime ... btime ...`, `go nodes N`, `go infinite` followed by `stop`, `ucinewgame`, `isready`, and `quit`. Moves use UCI coordinates such as `e2e4` or `e7e8q`. `go depth` and `go infinite` perform analysis without an opening-book lookup; timed searches may choose a book move.

The GUI can send these engine options:

```text
setoption name Hash value 64
setoption name OwnBook value true
setoption name BookIndex value opening_book.cbk
setoption name BookFile value opening_book.txt
setoption name Move Time Cap value 2000
setoption name Clear Hash
setoption name EvalFile value nn-134a887f4c8f.nnue
setoption name UseNNUE value true
```

`EvalFile` and `UseNNUE` are available in NNUE builds. Select a readable network path, then enable `UseNNUE`; false selects PeSTO. Successful evaluation changes clear the TT. A failed reload preserves the previously loaded network.

`Hash` is in MiB (1–1024). `OwnBook` defaults to true. The indexed book is read from `BookIndex` relative to the engine's working directory; if no valid index is found, the engine tries `BookFile` and its small built-in fallback. `Move Time Cap` defaults to 2000 milliseconds for clock-based searches; `0` removes that extra cap. `go movetime` specifies its own duration. These search times use process CPU time. The engine does not implement a UCI Elo or strength-limiting option.

## Play in the terminal

```powershell
.\cce_engine.exe --play
.\cce_engine.exe --play --color white --depth 15 --time-ms 5000 --nodes 10000000
.\cce_engine.exe --play --color black
.\cce_engine.exe --play --help
```

`--color` selects **your** color. The default is `black` (engine plays White). Enter your moves in UCI format at the prompt; enter `quit` to end the game. Per engine turn, the defaults are maximum depth 11, 2000 CPU milliseconds, and 5,000,000 nodes. `--time-ms 0` or `--nodes 0` removes the corresponding limit. A requested maximum depth need not be reached if another limit is met. Terminal play uses the search engine without the UCI opening-book selection.

## Solve a FEN puzzle

```powershell
.\cce_engine.exe --puzzle '8/1N2N3/2r5/3qp2R/QP2kp1K/5R2/6B1/6B1 w - - 0 1' --depth 30 --expect a4a8
.\cce_engine.exe --puzzle --fen '7k/8/5KQ1/8/8/8/8/8 w - - 0 1' --depth 10 --expect g6g7
```

Use double quotes instead of single quotes around FEN in CMD. Defaults are depth 15, 5000 CPU milliseconds, and 10,000,000 nodes. Override with `--depth N`, `--time-ms MS`, and `--nodes N`; zero removes a time/node cap. `--expect` checks one UCI move and prints PASS or FAIL. Exit codes are 0 for a match or a run without `--expect`, 1 for a mismatch, and 2 for invalid input. Output includes the best move, score, completed depth, nodes, and singular-extension counters. `Line (depth N)` shows the best principal variation from the last completed iteration in numbered UCI moves (for example `1. a4a8 d5d4 2. f3e3`; a Black-to-move FEN starts `24...`). `Advantage` identifies White or Black and gives the evaluation in pawns and centipawns from White’s perspective; forced mates are reported as mate in N. This is the searched best line, not every move examined. A mate search can finish before the requested depth once it has verified the reported mate distance. This mode reads **one FEN**, not a Lichess puzzle CSV. For a Lichess puzzle row, play the first move in its `Moves` field on the row's FEN to obtain the position in which the solver is to move.

## Opening book tools

Compile the separate indexed-book builder and build the book from one or more **UCI move-line text files**:

```powershell
gcc -O2 -std=c11 -Iinc tools\build_book.c src\bitboard.c src\movegen.c src\magic.c src\tt.c src\eval.c -o build_book.exe
.\build_book.exe opening_book.cbk opening_book.txt
```

`opening_book.cbk` is this engine's format, not a Polyglot book. Each builder run **replaces** the output index. Supply every input you want included each time. To convert Lichess openings TSV or standard-start PGN main lines into UCI move lines:

```powershell
python -m pip install chess
python tools\lichess_to_uci.py extra_book.txt chess-openings\a.tsv chess-openings\games.pgn
.\build_book.exe opening_book.cbk opening_book.txt extra_book.txt
```

The converter replaces its output text file and accepts up to 24 plies per line. `python-chess` is required for SAN PGN conversion; a TSV with a ready `uci` column does not require it. This converter is for **opening TSV/PGN**, not Lichess puzzle CSV.

## Diagnostics and tests

`python tools\build.py` builds the diagnostic and NNUE comparison programs separately from the engine:

```powershell
.\engine_diagnostics.exe --quick --depth 7
.\engine_diagnostics.exe --pruning --depth 8
.\engine_diagnostics.exe --singular --stress --depth 15
python tests\uci_smoke.py .\cce_engine.exe
python tests\book_index_test.py .\cce_engine.exe .\build_book.exe
.\nnue_benchmark.exe .\nn-134a887f4c8f.nnue --check
.\nnue_benchmark.exe .\nn-134a887f4c8f.nnue
```

The harness checks move and board restoration, perft, draw rules, and mate ordering before benchmarking. `--quick` is the default; `--stress` raises per-run budgets. `--pruning` compares selective pruning settings; `--singular` compares singular extensions off/on. `--depth N` sets the target (a `*` means the run stopped early). `--benchmark-only` skips the correctness checks when timing successive runs; run the checks at least once before using it. Benchmarks and puzzle results alone are **not an Elo rating**.

## Android

The existing `build_android` and OEX scripts build the C-only PeSTO engine. To build an NNUE ARM64 executable with the NDK, run:

```powershell
python tools\build.py --android-ndk "$env:ANDROID_NDK_HOME" --simd neon
```

Its output is `build/android/chess_engine_arm64`. Import it into a GUI such as DroidFish that supports standalone Android UCI executables, then set `EvalFile` to a network path readable by the engine and enable `UseNNUE`. This NDK path requires device verification on the target Android phone. The external 94 MiB network is not bundled by the APK scripts.

To build an ARM64 UCI executable for a GUI that imports binaries, install Android NDK and run `.\build_android.ps1` (or `./build_android.sh` on Linux/macOS). Its output is `build/android/chess_engine_arm64`, not a Windows executable. For **Chessis**, which supports OEX engines, install Android SDK Platform 35, Android NDK, and JDK 17+, then run:

```powershell
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
.\build_android_oex.ps1
adb install -r .\build\android\cce_chess_oex_debug.apk
```

Choose **CCE Chess Engine** among the installed OEX engines in Chessis. The APK builder also emits a standalone ARM64 binary. On Linux/macOS, set `ANDROID_HOME` and `ANDROID_NDK_HOME`, then run `./build_android_oex.sh`. Its Android application ID is `org.cce.oex`; Android treats it as a separate app from the earlier `org.codexchess.oex` package. The APK is a debug-signed personal test build; verify it on an ARM64 Android 8+ device. The bundled UCI engine has a small fallback opening book, but the external indexed book is not packaged into this APK.

## Engine design

- Bitboards with legal move validation, castling, en passant, promotion, and perft support.
- Stockfish NNUE with incremental feature updates and a selectable tapered PeSTO evaluator; iterative deepening with alpha-beta, quiescence search, move ordering and a transposition table.
- Aspiration windows, principal variation search, null-move pruning, late-move reductions, futility and reverse-futility pruning, razoring, and verified singular extensions. Selective methods are conditional; they do not fire on every position.
- Fifty-move and threefold-repetition handling with game history in terminal play and UCI.

For consistent results, compare completed depths and use the same executable, time/node settings, positions, and machine when measuring search changes.

## NNUE licensing

The NNUE-enabled executable incorporates GPLv3-or-later Stockfish source. Preserve the upstream authors and license notices, distribute under compatible GPLv3 terms, and provide corresponding source. See [upstream provenance](nnue/stockfish/UPSTREAM.md).

Move-state API and a pure C ply-stack example: [STATE_UPDATES.md](STATE_UPDATES.md).
