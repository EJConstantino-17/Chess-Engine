"""Compare CCE raw evaluations with an independent matching Stockfish binary."""
import argparse
import os
import queue
import re
import subprocess
import threading
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('benchmark', type=Path)
p.add_argument('stockfish', type=Path)
p.add_argument('network', type=Path)
a = p.parse_args()
network = a.network.resolve()
rows = subprocess.check_output([str(a.benchmark.resolve()), str(network), '--oracle'],
                               text=True, env={**os.environ, 'CCE_EVAL': 'pesto'}).splitlines()
engine = subprocess.Popen([str(a.stockfish.resolve())], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
lines = queue.Queue()
def read_output():
    for line in engine.stdout:
        lines.put(line.strip())
    lines.put(None)
threading.Thread(target=read_output, daemon=True).start()
def send(text):
    engine.stdin.write(text + '\n')
    engine.stdin.flush()
count = 0
try:
    send('setoption name Threads value 1')
    send('setoption name Hash value 16')
    send('setoption name EvalFile value ' + str(network))
    send('isready')
    while lines.get(timeout=30) != 'readyok':
        pass
    for row in rows:
        expected, fen = row.split('\t', 1)
        send('position fen ' + fen)
        send('eval')
        actual = None
        while True:
            line = lines.get(timeout=30)
            if line is None:
                raise RuntimeError('Stockfish exited during oracle test')
            match = re.search(r'NNUE evaluation\s+([+-]?\d+) \(side to move, internal units\)', line)
            if match:
                actual = int(match[1])
            if line.startswith('Final evaluation'):
                break
        if actual != int(expected):
            raise AssertionError(f'CCE {expected} != Stockfish {actual}: {fen}')
        count += 1
    assert count >= 1000, f'Only {count} positions tested'
    print(f'PASS: {count} positions match Stockfish exactly in raw internal units.')
finally:
    if engine.poll() is None:
        send('quit')
        engine.wait(timeout=10)
