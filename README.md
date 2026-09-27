# C Chess Engine

Chess Engine written in C utilizing **Bitboard** representation.

## Prerequisites

Make sure you have a C compiler (`gcc`) and `make` installed on your system:

- **Windows**: [MinGW-w64](https://www.winlibs.com/) (provides `gcc` and `mingw32-make`) or MSYS2
- **Linux**: `sudo apt install build-essential`
- **macOS**: `xcode-select --install`

## Building and Running

### Compile
```bash
# Windows (PowerShell / CMD)
mingw32-make clean; mingw32-make

# Linux / macOS / Termux
make clean && make

WIP