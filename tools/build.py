"""9/30/2026 12:05: Portable C/C++ build for PowerShell, Linux, Termux, and Android NDK."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import platform
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--pesto-only', action='store_true')
p.add_argument('--simd', choices=['scalar', 'avx2', 'neon'], default='scalar')
p.add_argument('--android-ndk', type=Path)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--output-dir', type=Path)
p.add_argument('--jobs', type=int, default=4)
a = p.parse_args()
if a.jobs < 1:
    p.error('--jobs must be positive')
if a.pesto_only and a.simd != 'scalar':
    p.error('SIMD is only used by NNUE')
if a.android_ndk and a.simd == 'avx2':
    p.error('Android ARM64 requires scalar or neon')
common = ['-O3', '-Wall', '-Wextra', '-D_GNU_SOURCE', '-I' + str(root / 'inc')]
link_flags = ['-Wl,-dead_strip'] if platform.system() == 'Darwin' and not a.android_ndk else ['-Wl,--gc-sections']
if a.android_ndk:
    host = {'Windows': 'windows-x86_64', 'Darwin': 'darwin-x86_64', 'Linux': 'linux-x86_64'}[platform.system()]
    toolchain = a.android_ndk / 'toolchains/llvm/prebuilt' / host / 'bin'
    suffix = '.exe' if os.name == 'nt' else ''
    cc, cxx = str(toolchain / ('clang' + suffix)), str(toolchain / ('clang++' + suffix))
    common += ['--target=aarch64-linux-android26', '-fPIE']
    link_flags += ['--target=aarch64-linux-android26', '-pie', '-static-libstdc++']
    output_dir = root / 'build/android'
    exe_suffix = ''
else:
    cc, cxx = os.environ.get('CC', 'gcc'), os.environ.get('CXX', 'g++')
    output_dir = root
    exe_suffix = '.exe' if os.name == 'nt' else ''
for compiler in [cc] + ([] if a.pesto_only else [cxx]):
    if not shutil.which(compiler):
        p.error(f'Compiler not found: {compiler}. Install a 64-bit GCC/G++ or Android NDK toolchain.')
if a.sanitize:
    common += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    link_flags += ['-fsanitize=address,undefined']
mode = ('android' if a.android_ndk else 'native') + '-' + ('pesto' if a.pesto_only else a.simd) + ('-asan' if a.sanitize else '')
build = root / 'build' / mode
if a.output_dir:
    output_dir = a.output_dir.resolve()
output_dir.mkdir(parents=True, exist_ok=True)
c_sources = sorted((root / 'src').glob('*.c'))
core_sources = [s for s in c_sources if s.name not in ['main.c', 'uci.c', 'game_loop.c', 'puzzle.c', 'opening_book.c']]
sf = root / 'nnue/stockfish/src'
cpp_sources = []
if not a.pesto_only:
    common += ['-DCCE_NNUE']
    cpp_sources = [sf / n for n in ['attacks.cpp', 'position.cpp', 'misc.cpp', 'memory.cpp',
                   'nnue/network.cpp', 'nnue/nnue_accumulator.cpp']]
    cpp_sources += sorted((sf / 'nnue/features').glob('*.cpp'))
    cpp_sources += [root / 'nnue/cce_nnue.cpp', root / 'nnue/sha256.cpp']
    c_sources.append(root / 'nnue/network_file.c')
cpp_flags = ['-std=c++17', '-ffunction-sections', '-fdata-sections', '-DNNUE_EMBEDDING_OFF', '-DCCE_NNUE_ONLY', '-DIS_64BIT']
if 'clang' in Path(cxx).name:
    cpp_flags += ['-fconstexpr-steps=500000000']
else:
    cpp_flags += ['-fconstexpr-ops-limit=500000000']
if not a.sanitize:
    cpp_flags += ['-DNDEBUG']
if a.simd == 'avx2':
    cpp_flags += ['-mavx2', '-DUSE_AVX2', '-DUSE_SSE2', '-DUSE_SSSE3', '-DUSE_SSE41']
if a.simd == 'neon':
    cpp_flags += ['-DUSE_NEON']
tests = [] if a.android_ndk else [root / 'tests/engine_diagnostics.c']
if not a.pesto_only and not a.android_ndk:
    tests.append(root / 'tests/nnue_benchmark.c')
objects = {}
def compile_source(source):
    obj = (build / source.relative_to(root)).with_suffix('.o')
    obj.parent.mkdir(parents=True, exist_ok=True)
    cpp = source.suffix == '.cpp'
    command = [cxx if cpp else cc, *common, *(cpp_flags if cpp else ['-std=c11']), '-c', str(source), '-o', str(obj)]
    subprocess.run(command, check=True)
    return source, obj
with ThreadPoolExecutor(max_workers=a.jobs) as pool:
    objects.update(pool.map(compile_source, c_sources + cpp_sources + tests))
linker = cc if a.pesto_only else cxx
nnue_sources = [] if a.pesto_only else cpp_sources + [root / 'nnue/network_file.c']
def link(name, sources):
    output = output_dir / (name + exe_suffix)
    subprocess.run([linker, *[str(objects[s]) for s in sources], *link_flags, '-o', str(output)], check=True)
    print(f'Built {output}')
link('chess_engine_arm64' if a.android_ndk else 'cce_engine', c_sources + cpp_sources)
if not a.android_ndk:
    link('engine_diagnostics', [tests[0], *core_sources, *nnue_sources])
    if not a.pesto_only:
        link('nnue_benchmark', [tests[1], *core_sources, *nnue_sources])
