"""Run: python tests/ponder_uci_test.py ENGINE [NETWORK]. No extra packages needed."""
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time

engine = str(Path(sys.argv[1]).resolve())
env = dict(os.environ)
if len(sys.argv) > 2:
    env['CCE_NNUE_FILE'] = str(Path(sys.argv[2]).resolve())
p = subprocess.Popen([engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE, text=True, bufsize=1, env=env)
q = queue.Queue()
threading.Thread(target=lambda: [q.put(s.strip()) for s in p.stdout], daemon=True).start()
errors = []
threading.Thread(target=lambda: errors.extend(p.stderr.readlines()), daemon=True).start()
checks = 0

def send(s):
    p.stdin.write(s + '\n')
    p.stdin.flush()

def until(prefix, timeout=5):
    rows = []
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            s = q.get(timeout=min(.1, max(.001, end-time.monotonic())))
        except queue.Empty:
            continue
        rows.append(s)
        if s.startswith(prefix):
            return rows
    raise AssertionError((prefix, rows, errors))

def silent(seconds=.15):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        try:
            s = q.get(timeout=.01)
        except queue.Empty:
            continue
        assert not s.startswith('bestmove '), ('premature result', s)

def best():
    rows = until('bestmove ')
    assert rows[-1].split()[1] != '0000', rows
    return rows

def checked(name):
    global checks
    checks += 1
    print('PASS:', name)

try:
    send('uci'); rows = until('uciok')
    assert 'option name Ponder type check default false' in rows
    if len(sys.argv) > 2:
        assert 'option name UseNNUE type check default true' in rows
    send('setoption name OwnBook value false')
    send('setoption name Ponder value true')
    send('setoption name Move Time Cap value 0')
    send('position startpos'); send('go depth 4')
    baseline = best()
    assert len(baseline[-1].split()) == 4 and baseline[-1].split()[2] == 'ponder'
    checked('Ponder option and predicted PV reply')

    # A completed fixed-depth ponder must not publish early, even past movetime.
    send('setoption name Clear Hash'); send('position startpos')
    send('go ponder depth 3 movetime 40'); silent(.2)
    send('isready'); until('readyok'); silent(.05)
    send('ponderhit'); best(); silent(.03)
    checked('finished ponder waits; isready responsive; hit publishes exactly once')

    send('position startpos'); send('go ponder movetime 80'); silent(.18)
    started = time.monotonic(); send('ponderhit'); rows = best()
    elapsed = time.monotonic()-started
    assert .025 <= elapsed < 1.5, elapsed
    assert int(re.search(r' nodes (\d+)', rows[-2]).group(1)) > 0
    checked(f'active ponder continues on hit, new clock budget ({elapsed*1000:.1f} ms)')

    send('position startpos'); send('go ponder nodes 1'); silent()
    send('stop'); best(); silent(.03)
    checked('node-limited ponder and stop return a legal fallback')

    send('position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1')
    send('go ponder depth 4'); silent(); send('isready'); until('readyok')
    send('ponderhit'); assert until('bestmove ')[-1] == 'bestmove 0000'
    checked('terminal position waits for hit')

    send('position startpos'); send('go infinite depth 2'); silent()
    send('ponderhit'); silent(.05); send('stop'); best()
    checked('infinite search retains results until stop; stray hit ignored')

    # Wrong prediction: finish old search, replace the root, start a new one.
    send('position startpos moves e2e4 e7e5'); send('go ponder'); silent(.05)
    send('stop'); best()
    send('position startpos moves e2e4 c7c5'); send('go depth 3'); best()
    send('ucinewgame'); send('setoption name Clear Hash')
    send('position startpos'); send('go depth 4'); restored = best()
    normalize = lambda rows: [re.sub(r' (?:time|nps) \d+', '', x) for x in rows]
    assert normalize(restored) == normalize(baseline), (restored, baseline)
    checked('ponder miss, board/NNUE restoration and deterministic normal search')

    send('setoption name OwnBook value true'); send('position startpos')
    send('go ponder depth 2 wtime 60000 btime 60000'); silent()
    send('stop'); best()
    send('position startpos'); send('go wtime 60000 btime 60000')
    rows = best(); assert any(x.startswith('info string book move') for x in rows)
    checked('ponder bypasses instant book output; ordinary book still works')

    send('setoption name OwnBook value false'); send('setoption name Ponder value false')
    send('position startpos'); send('go movetime 50')
    assert len(best()[-1].split()) == 2
    checked('Ponder false omits predicted reply; ordinary timed search works')

    send('position startpos'); send('go ponder'); silent(.05)
    send('quit'); assert p.wait(timeout=3) == 0; silent(.03)
    assert not any('NNUE load failed' in x for x in errors), errors
    checked('quit during active ponder exits without bestmove')
    print(f'{checks} pondering checks passed')
finally:
    if p.poll() is None:
        p.kill(); p.wait()
