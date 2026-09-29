"""Run: python3 tests/uci_smoke.py ./chess_engine"""
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path

p = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, text=True, bufsize=1)
lines = queue.Queue()
threading.Thread(target=lambda: [lines.put(line.strip()) for line in p.stdout],
                 daemon=True).start()

def send(line):
    p.stdin.write(line + '\n')
    p.stdin.flush()

def expect(prefix):
    end = time.monotonic() + 10
    while time.monotonic() < end:
        try:
            line = lines.get(timeout=0.2)
        except queue.Empty:
            continue
        if line.startswith(prefix):
            print(line)
            return line
    raise AssertionError('missing: ' + prefix)

try:
    send('uci'); expect('uciok')
    send('isready'); expect('readyok')
    # timed openings are immediate; multiple variations appear.
    seen = set()
    for _ in range(24):
        send('position startpos')
        send('go wtime 300000 btime 300000')
        book_line = expect('info string book move ')
        reply = expect('bestmove ')
        assert book_line.split()[-1] == reply.split()[-1]
        seen.add(reply)
    assert len(seen) >= 2, f'book did not diversify: {seen}'
    send('position startpos moves e2e4')
    send('go wtime 300000 btime 300000')
    expect('info string book move ')
    assert expect('bestmove ') != 'bestmove 0000'
    # Every published opening prefix must yield a verified book continuation.
    for text in (Path(__file__).resolve().parent.parent / 'opening_book.txt').read_text().splitlines():
        if not text or text.startswith('#'):
            continue
        sequence = text.split()
        for n in range(len(sequence)):
            prefix = ' moves ' + ' '.join(sequence[:n]) if n else ''
            send('position startpos' + prefix)
            send('go wtime 300000 btime 300000')
            expect('info string book move ')
            assert expect('bestmove ') != 'bestmove 0000'
    send('setoption name BookIndex value no_such_book_index.cbk')
    send('setoption name BookFile value no_such_book_file.txt')
    send('position startpos')
    send('go wtime 300000 btime 300000')
    expect('info string book move ')  # bundled fallback for DroidFish
    assert expect('bestmove ') != 'bestmove 0000'
    send('setoption name BookIndex value opening_book.cbk')
    send('setoption name BookFile value opening_book.txt')
    # TIME CAP: an explicit short cap still returns a legal move off-book.
    send('setoption name OwnBook value false')
    send('position fen r6k/pp2r2p/4Rp1Q/3p4/8/1N1P2R1/PqP2bPP/7K b - - 0 24')
    start = time.monotonic()
    send('go depth 30 wtime 300000 btime 300000')
    expect('info depth ')
    assert expect('bestmove ') != 'bestmove 0000'
    assert time.monotonic() - start < 3.5  # default two-second cap + overhead
    send('setoption name Move Time Cap value 120')
    start = time.monotonic()
    send('go depth 30 wtime 300000 btime 300000')
    expect('info depth ')
    assert expect('bestmove ') != 'bestmove 0000'
    assert time.monotonic() - start < 2.0
    send('setoption name Move Time Cap value 2000')
    send('position startpos moves e2e4 e7e5 g1f3')
    send('go depth 3'); assert expect('bestmove ') != 'bestmove 0000'
    send('position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1')
    send('go depth 2'); assert expect('bestmove ') == 'bestmove 0000'
    send('position startpos'); send('go infinite')
    time.sleep(0.1); send('stop')
    assert expect('bestmove ') != 'bestmove 0000'
    send('quit'); assert p.wait(timeout=3) == 0
    print('UCI smoke checks passed')
finally:
    if p.poll() is None:
        p.kill(); p.wait()
