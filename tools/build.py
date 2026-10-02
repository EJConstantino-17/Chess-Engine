
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
p.add_argument('--simd', choices=['auto', 'scalar', 'avx2', 'neon'], default='auto')
p.add_argument('--android', '--android-arm64', action='store_true',
               help='Build an Android ARM64 UCI executable using the Android NDK.')
p.add_argument('--android-ndk', type=Path, help='NDK directory; also enables the Android ARM64 build.')
p.add_argument('--android-api', type=int, default=26, help='Android API level (minimum 26; default 26).')
p.add_argument('--termux', action='store_true', help='Build directly in ARM64 Termux using its Clang toolchain; no NDK needed.')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--nnue-profile', action='store_true', help='Count accumulator paths for diagnostics.')
p.add_argument('--output-dir', type=Path)
p.add_argument('--jobs', type=int, default=4)
a = p.parse_args()
if a.termux and (a.android or a.android_ndk):
    p.error('--termux cannot be combined with --android or --android-ndk')
if a.android or a.android_ndk:
    ndk_path = a.android_ndk or os.environ.get('ANDROID_NDK_HOME') or os.environ.get('ANDROID_NDK_ROOT')
    if not ndk_path:
        p.error('--android requires --android-ndk PATH or ANDROID_NDK_HOME/ANDROID_NDK_ROOT')
    a.android_ndk = Path(ndk_path).expanduser().resolve()
    if a.android_api < 26:
        p.error('Android ARM64 builds require API 26 or newer')
elif a.android_api != 26:
    p.error('--android-api requires an NDK Android build; Termux uses its installed compiler API')
if a.jobs < 1:
    p.error('--jobs must be positive')
if a.pesto_only and a.simd not in ('auto', 'scalar'):
    p.error('SIMD is only used by NNUE')
if (a.android_ndk or a.termux) and a.simd == 'avx2':
    p.error('Android ARM64 requires scalar or neon')
common = ['-O3', '-Wall', '-Wextra', '-D_GNU_SOURCE', '-I' + str(root / 'inc')]
link_flags = ['-Wl,-dead_strip'] if platform.system() == 'Darwin' and not a.android_ndk else ['-Wl,--gc-sections']
if a.android_ndk:
    host = {'Windows': 'windows-x86_64', 'Darwin': 'darwin-x86_64', 'Linux': 'linux-x86_64'}[platform.system()]
    toolchain = a.android_ndk / 'toolchains/llvm/prebuilt' / host / 'bin'
    suffix = '.exe' if os.name == 'nt' else ''
    cc, cxx = str(toolchain / ('clang' + suffix)), str(toolchain / ('clang++' + suffix))
    target = f'aarch64-linux-android{a.android_api}'
    common += ['--target=' + target, '-fPIE']
    link_flags += ['--target=' + target, '-pie', '-static-libstdc++']
    output_dir = root / 'build/android'
    exe_suffix = ''
elif a.termux:
    cc, cxx = os.environ.get('CC', 'clang'), os.environ.get('CXX', 'clang++')
    common += ['-fPIE']
    link_flags += ['-pie']
    output_dir = root / 'build/termux'
    exe_suffix = ''
else:
    cc, cxx = os.environ.get('CC', 'gcc'), os.environ.get('CXX', 'g++')
    output_dir = root
    exe_suffix = '.exe' if os.name == 'nt' else ''
for compiler in [cc] + ([] if a.pesto_only else [cxx]):
    if not shutil.which(compiler):
        p.error(f'Compiler not found: {compiler}. In Termux run: pkg install clang python. Otherwise install GCC/G++ or the Android NDK.')
if a.termux:
    for compiler in [cc] + ([] if a.pesto_only else [cxx]):
        probe = subprocess.run([compiler, '-dumpmachine'], text=True, capture_output=True)
        target = probe.stdout.strip().lower()
        if probe.returncode or not target.startswith('aarch64') or 'android' not in target:
            p.error('--termux requires ARM64 Android Clang; run this option inside ARM64 Termux')
if a.simd == 'auto':
    a.simd = 'scalar'
    if not a.pesto_only:
        if a.android_ndk or a.termux:
            a.simd = 'neon'
        else:
            probe = subprocess.run([cxx, '-march=native', '-dM', '-E', '-x', 'c++', '-'],
                                   input='', text=True, capture_output=True)
            if probe.returncode == 0:
                if '#define __AVX2__ ' in probe.stdout:
                    a.simd = 'avx2'
                elif '#define __ARM_NEON' in probe.stdout:
                    a.simd = 'neon'
            else:
                print('CPU feature probe unavailable; using portable scalar NNUE.')
print('Evaluation build: ' + ('PeSTO' if a.pesto_only else 'NNUE ' + a.simd.upper()))
if a.sanitize:
    common += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    link_flags += ['-fsanitize=address,undefined']
target_mode = f'android-arm64-api{a.android_api}' if a.android_ndk else ('termux-arm64' if a.termux else 'native')
mode = target_mode + '-' + ('pesto' if a.pesto_only else a.simd) + ('-asan' if a.sanitize else '')
if a.nnue_profile:
    mode += '-profile'
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
if a.nnue_profile:
    cpp_flags += ['-DCCE_NNUE_PROFILE']
if not a.sanitize:
    cpp_flags += ['-DNDEBUG']
if a.simd == 'avx2':
    cpp_flags += ['-mavx2', '-DUSE_AVX2', '-DUSE_SSE2', '-DUSE_SSSE3', '-DUSE_SSE41']
if a.simd == 'neon':
    cpp_flags += ['-DUSE_NEON']
tests = [] if a.android_ndk else [root / 'tests/engine_diagnostics.c']
if not a.pesto_only and not a.android_ndk:
    tests.append(root / 'tests/nnue_benchmark.c')
    tests.append(root / 'tests/search_tree_benchmark.c')
    tests.append(root / 'tests/tactical_probe.c')
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
link('cce_engine_arm64' if a.android_ndk or a.termux else 'cce_engine', c_sources + cpp_sources)
if not a.android_ndk:
    link('engine_diagnostics', [tests[0], *core_sources, *nnue_sources])
    if not a.pesto_only:
        link('nnue_benchmark', [tests[1], *core_sources, *nnue_sources])

if not a.android_ndk and not a.pesto_only:
    link('search_tree_benchmark', [tests[2], *core_sources, *nnue_sources])
    link('tactical_probe', [tests[3], *core_sources, *nnue_sources])
