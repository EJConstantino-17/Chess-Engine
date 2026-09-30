"""9/30/2026 15:29: Timed off-book searches verify UCI time/NPS/PV and short budgets."""
import argparse
import os
from pathlib import Path
import queue
import re
import subprocess
import threading
import time

p = argparse.ArgumentParser()
p.add_argument('engine', type=Path)
p.add_argument('network', type=Path)
p.add_argument('--baseline', action='store_true', help='Allow missing time/NPS in the old engine.')
a = p.parse_args()
engine = subprocess.Popen([str(a.engine.resolve()), '--uci'], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1,
                          env={**os.environ, 'CCE_NNUE_FILE': str(a.network.resolve())})
lines = queue.Queue()
threading.Thread(target=lambda: [lines.put(line.strip()) for line in engine.stdout], daemon=True).start()
def send(s):
    engine.stdin.write(s + '\n')
    engine.stdin.flush()
def until(prefix):
    result = []
    while True:
        line = lines.get(timeout=20)
        result.append(line)
        if line.startswith(prefix):
            return result
try:
    send('uci')
    handshake = until('uciok')
    print(next((s for s in handshake if s.startswith('info string NNUE compute backend')), 'profile: old build'))
    send('setoption name OwnBook value false')
    send('setoption name Move Time Cap value 0')
    send('isready'); until('readyok')
    for label, position in [
        ('opening', 'startpos moves e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 d2d3 f8c5'),
        ('tactic', 'fen r6k/pp2r2p/4Rp1Q/3p4/8/1N1P2R1/PqP2bPP/7K b - - 0 24')
    ]:
        for command, budget in [('go movetime 50', 50),
                                ('go wtime 5000 btime 5000 winc 50 binc 50', 191)]:
            send('setoption name Clear Hash')
            send('position ' + position)
            begin = time.perf_counter()
            send(command)
            rows = until('bestmove ')
            elapsed = (time.perf_counter() - begin) * 1000
            assert rows[-1] != 'bestmove 0000', rows
            info = next(s for s in reversed(rows) if s.startswith('info depth '))
            depth = int(re.search(r'\bdepth (\d+)', info)[1])
            nodes = int(re.search(r'\bnodes (\d+)', info)[1])
            if not a.baseline:
                assert re.search(r'\btime \d+ nps \d+', info), info
                assert int(re.search(r'\bnps (\d+)', info)[1]) > 0
                assert abs(int(re.search(r'\btime (\d+)', info)[1]) - elapsed) < 60
                if depth:
                    assert ' pv ' in info
                assert elapsed < budget + 100, (label, elapsed, budget)
            print(f'{label},budget_ms={budget},wall_ms={elapsed:.2f},depth={depth},nodes={nodes},'
                  f'observed_nps={nodes * 1000 / elapsed:.0f}')
    print('PASS: timed searches and UCI metrics')
finally:
    if engine.poll() is None:
        send('quit')
        engine.wait(timeout=5)
