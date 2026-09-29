# C Chess Engine

A single-threaded chess engine written in C with bitboard move generation, PeSTO-style tapered evaluation, an iterative-deepening search, and the Universal Chess Interface (UCI). The program also supports a terminal game and a standalone FEN puzzle check. Run commands from the project root, alongside `src/`, `inc/`, `tools/`, and `tests/`.

## Requirements and build

Install a C11 compiler (GCC or Clang). On Windows, MinGW-w64 supplies `gcc` and `mingw32-make`; Python 3 is needed only for the conversion and test scripts. If your project has its Makefile, build in PowerShell:

```powershell
mingw32-make clean
mingw32-make
```

The engine target must compile all `src/*.c`, including `src/puzzle.c`, and link them into `cce_engine.exe`. Keep `tools/build_book.c` and `tests/engine_diagnostics.c` out of the engine target: each has its own `main()`. Without a Makefile, build directly:

```powershell
gcc -O3 -std=c11 -Iinc src\*.c -o cce_engine.exe
```

or in linux/termux:

```powershell
clang -O3 -Wall src/*.c -o cce_engine
```

On Linux/macOS, use `make clean && make` if a compatible Makefile is supplied, or `cc -O3 -std=c11 -Iinc src/*.c -o cce_engine`. Replace `cce_engine.exe` with `./cce_engine` in the examples below. Close running engine processes before replacing their executable on Windows. Keep matching versions of all `src` and `inc` files together.

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
```

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

Use double quotes instead of single quotes around FEN in CMD. Defaults are depth 15, 5000 CPU milliseconds, and 10,000,000 nodes. Override with `--depth N`, `--time-ms MS`, and `--nodes N`; zero removes a time/node cap. `--expect` checks one UCI move and prints PASS or FAIL. Exit codes are 0 for a match or a run without `--expect`, 1 for a mismatch, and 2 for invalid input. Output includes best move, score, completed depth, nodes, and singular-extension counters. A mate search can finish before the requested depth once it has verified the reported mate distance. This mode reads **one FEN**, not a Lichess puzzle CSV. For a Lichess puzzle row, play the first move in its `Moves` field on the row's FEN to obtain the position in which the solver is to move.

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

Build the diagnostic program separately from the engine; its source belongs at `tests/engine_diagnostics.c`:

```powershell
gcc -O2 -std=c11 -Iinc tests\engine_diagnostics.c src\bitboard.c src\eval.c src\magic.c src\movegen.c src\perft.c src\search.c src\tt.c -o engine_diagnostics.exe
.\engine_diagnostics.exe --quick --depth 7
.\engine_diagnostics.exe --pruning --depth 8
.\engine_diagnostics.exe --singular --stress --depth 15
python tests\uci_smoke.py .\cce_engine.exe
python tests\book_index_test.py .\cce_engine.exe .\build_book.exe
```

The harness checks move and board restoration, perft, draw rules, and mate ordering before benchmarking. `--quick` is the default; `--stress` raises per-run budgets. `--pruning` compares selective pruning settings; `--singular` compares singular extensions off/on. `--depth N` sets the target (a `*` means the run stopped early). `--benchmark-only` skips the correctness checks when timing successive runs; run the checks at least once before using it. Benchmarks and puzzle results alone are **not an Elo rating**.

## Android

To build an ARM64 UCI executable for a GUI that imports binaries, install Android NDK and run `.\build_android.ps1` (or `./build_android.sh` on Linux/macOS). Its output is `build/android/chess_engine_arm64`, not a Windows executable. For **Chessis**, which supports OEX engines, install Android SDK Platform 35, Android NDK, and JDK 17+, then run:

```powershell
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
.\build_android_oex.ps1
adb install -r .\build\android\cce_chess_oex_debug.apk
```

Choose **CCE Chess Engine** among the installed OEX engines in Chessis. The APK builder also emits a standalone ARM64 binary. On Linux/macOS, set `ANDROID_HOME` and `ANDROID_NDK_HOME`, then run `./build_android_oex.sh`. The APK is a debug-signed personal test build; verify it on an ARM64 Android 8+ device. The bundled UCI engine has a small fallback opening book, but the external indexed book is not packaged into this APK.

## Engine design

- Bitboards with legal move validation, castling, en passant, promotion, and perft support.
- Tapered PeSTO-style static evaluation; iterative deepening with alpha-beta, quiescence search, move ordering and a transposition table.
- Aspiration windows, principal variation search, null-move pruning, late-move reductions, futility and reverse-futility pruning, razoring, and verified singular extensions. Selective methods are conditional; they do not fire on every position.
- Fifty-move and threefold-repetition handling with game history in terminal play and UCI.

For consistent results, compare completed depths and use the same executable, time/node settings, positions, and machine when measuring search changes.
