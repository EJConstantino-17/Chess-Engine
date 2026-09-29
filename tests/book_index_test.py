"""Run: python tests/book_index_test.py ./chess_engine ./build_book"""
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

engine, builder = sys.argv[1:3]
with tempfile.TemporaryDirectory() as temp:
    text = Path(temp) / 'large.txt'
    index = Path(temp) / 'large.cbk'
    # 15,000 lines; b1c3 is absent from the bundled book and standard text.
    text.write_text(('b1c3 d7d5 e2e4 g8f6\n' * 15000) +
                    'e2e4 e7e5 g1f3 b8c6 f1c4\n', encoding='ascii')
    built = subprocess.run([builder, str(index), str(text)], capture_output=True,
                           text=True, check=True)
    assert 'accepted=15001' in built.stdout, built.stdout
    assert index.stat().st_size < 1024  # indexed/aggregated, not a 15k-line scan
    p = subprocess.Popen([engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         text=True, bufsize=1)
    responses = queue.Queue()
    threading.Thread(target=lambda: [responses.put(line.strip()) for line in p.stdout],
                     daemon=True).start()

    def send(s):
        p.stdin.write(s + '\n')
        p.stdin.flush()

    def wait(prefix):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                line = responses.get(timeout=0.2)
            except queue.Empty:
                continue
            if line.startswith(prefix):
                return line
        raise AssertionError(f'missing {prefix}')

    try:
        send('uci'); wait('uciok')
        send('setoption name BookFile value nonexistent.txt')
        send('setoption name BookIndex value ' + str(index))
        start = time.monotonic()
        send('position startpos'); send('go wtime 300000 btime 300000')
        assert wait('info string book move ') == 'info string book move b1c3'
        assert wait('bestmove ') == 'bestmove b1c3'
        assert time.monotonic() - start < 1
        send('position startpos moves b1c3')
        send('go wtime 300000 btime 300000')
        assert wait('info string book move ') == 'info string book move d7d5'
        assert wait('bestmove ') == 'bestmove d7d5'
        # A different move order reaches the same Zobrist position.
        send('position startpos moves g1f3 b8c6 e2e4 e7e5')
        send('go wtime 300000 btime 300000')
        assert wait('info string book move ') == 'info string book move f1c4'
        assert wait('bestmove ') == 'bestmove f1c4'
        send('quit'); assert p.wait(timeout=3) == 0
        print('indexed 15,000-line book: PASS, rapid lookup and correct moves')
    finally:
        if p.poll() is None:
            p.kill(); p.wait()
