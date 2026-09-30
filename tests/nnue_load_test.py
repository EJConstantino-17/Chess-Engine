"""Hash vectors, malformed-file rejection, and transactional reload."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
p = argparse.ArgumentParser()
p.add_argument('benchmark', type=Path)
p.add_argument('network', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    temp = Path(tmp)
    source = temp / 'hash_test.cpp'
    source.write_text('#include "sha256.h"\n#include <iostream>\nint main(int n,char **v) { if(n!=2)return 2;std::cout<<cce_file_sha256(v[1]); }\n')
    exe = temp / ('hash_test.exe' if os.name == 'nt' else 'hash_test')
    subprocess.run([os.environ.get('CXX', 'g++'), '-O2', '-std=c++17', '-I' + str(root / 'nnue'),
                    str(source), str(root / 'nnue/sha256.cpp'), '-o', str(exe)], check=True)
    for length in [0, 3, 55, 56, 63, 64, 65, 65535, 65536, 65537, 131072]:
        payload = b'abc' if length == 3 else bytes(i % 251 for i in range(length))
        sample = temp / 'hash.bin'
        sample.write_bytes(payload)
        actual = subprocess.check_output([str(exe), str(sample)], text=True)
        assert actual == hashlib.sha256(payload).hexdigest(), (length, actual)
    bad = temp / 'bad.nnue'
    for damage in ['short', 'version', 'architecture', 'description', 'weights', 'trailing']:
        if damage == 'short':
            bad.write_bytes(b'not a network')
        else:
            shutil.copyfile(a.network, bad)
            if damage == 'trailing':
                with bad.open('ab') as stream: stream.write(b'x')
            else:
                offset = {'version': 0, 'architecture': 4, 'description': 8, 'weights': 50000}[damage]
                with bad.open('r+b') as stream:
                    stream.seek(offset)
                    byte = stream.read(1)
                    stream.seek(offset)
                    stream.write(bytes([byte[0] ^ 0x80]))
        subprocess.run([str(a.benchmark.resolve()), str(a.network.resolve()), '--reject', str(bad)],
                       env={**os.environ, 'CCE_EVAL': 'pesto'}, check=True)
print('PASS: 11 SHA-256 vectors and 6 malformed-file/reload tests.')
