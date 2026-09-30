"""Check the supplied network and reject damaged headers."""
import hashlib
import pathlib
import subprocess
import sys
import tempfile

engine_root = pathlib.Path(__file__).resolve().parents[1]
network = pathlib.Path(sys.argv[1]).resolve()
expected = '134a887f4c8ff7bf7284177a3b3fc6ff9cef95ba89eb8db3079a8e507f7126af'
with network.open('rb') as source:
    digest = hashlib.file_digest(source, 'sha256').hexdigest()
assert digest == expected, f'Unexpected SHA-256: {digest}'
with tempfile.TemporaryDirectory() as tmp:
    checker = pathlib.Path(tmp) / 'nnue_check'
    subprocess.run(['cc', '-O2', '-std=c11', str(engine_root / 'nnue/network_file.c'),
                    str(engine_root / 'nnue/network_check.c'), '-o', str(checker)], check=True)
    assert subprocess.run([str(checker), str(network)], capture_output=True).returncode == 0
    bad = pathlib.Path(tmp) / 'bad.nnue'
    bad.write_bytes(b'not a network')
    assert subprocess.run([str(checker), str(bad)], capture_output=True).returncode != 0
print('Stockfish network SHA-256 and container inspection passed; run nnue_benchmark and nnue_oracle.py to validate inference.')
