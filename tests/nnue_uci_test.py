"""Exercise backend switching and failed network reloads through UCI."""
import argparse
import os
from pathlib import Path
import queue
import subprocess
import threading
p = argparse.ArgumentParser()
p.add_argument('engine', type=Path)
p.add_argument('network', type=Path)
a = p.parse_args()
engine = subprocess.Popen([str(a.engine.resolve())], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True, bufsize=1,
                          env={**os.environ, 'CCE_NNUE_FILE': str(a.network.resolve())})
q = queue.Queue()
threading.Thread(target=lambda: [q.put(line.strip()) for line in engine.stdout], daemon=True).start()
def send(text):
    engine.stdin.write(text + '\n');engine.stdin.flush()
def collect(prefix):
    result=[]
    while True:
        line=q.get(timeout=30);result.append(line)
        if line.startswith(prefix): return result
def search():
    send('position startpos moves e2e4 e7e5 g1f3 b8c6')
    send('go depth 4')
    rows=collect('bestmove ')
    assert rows[-1] != 'bestmove 0000'
    return rows
try:
    send('uci');assert 'option name UseNNUE type check default true' in collect('uciok')
    send('setoption name OwnBook value false')
    send('setoption name Move Time Cap value 0')
    send('setoption name Clear Hash');baseline=search()
    send('setoption name EvalFile value /no/such/network.nnue')
    assert any('NNUE load failed' in row for row in collect('info string NNUE load failed'))
    send('setoption name Clear Hash');assert search()==baseline
    send('setoption name UseNNUE value false');assert search()!=baseline
    send('setoption name UseNNUE value true');assert search()==baseline
    send('ucinewgame');send('isready');collect('readyok')
    assert search()==baseline
    send('quit');assert engine.wait(timeout=10)==0
    print('PASS: UCI NNUE/PeSTO switching, failed reload preservation, and new-game reset.')
finally:
    if engine.poll() is None: engine.kill();engine.wait()
