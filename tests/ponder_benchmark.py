"""Compare normal search and extra depth from pondering; outputs CSV.
python tests/ponder_benchmark.py BEFORE AFTER NETWORK > pondering.csv
"""
import csv
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time

before, after, network = [str(Path(x).resolve()) for x in sys.argv[1:4]]
positions = [('startpos', 'position startpos'),
             ('development', 'position startpos moves e2e4 e7e5 g1f3 b8c6 f1c4 g8f6'),
             ('kiwipete', 'position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1')]
writer = csv.writer(sys.stdout)
writer.writerow(['position', 'mode', 'repeat', 'depth', 'nodes', 'score_type', 'score', 'bestmove', 'own_turn_ms', 'ponder_ms'])
for name, position in positions:
    for mode in ['before-depth6', 'after-depth6', 'cold100', 'ponder250-hit100']:
        for repeat in range(3):
            path = before if mode.startswith('before') else after
            p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.DEVNULL, text=True, bufsize=1,
                                 env={**os.environ, 'CCE_NNUE_FILE': network})
            q = queue.Queue()
            threading.Thread(target=lambda stream=p.stdout, inbox=q: [inbox.put(x.strip()) for x in stream], daemon=True).start()
            def send(s):
                p.stdin.write(s+'\n');p.stdin.flush()
            def wait(prefix):
                rows=[]
                while True:
                    x=q.get(timeout=10); rows.append(x)
                    if x.startswith(prefix):return rows
            try:
                send('uci');wait('uciok');send('setoption name OwnBook value false')
                send(position)
                if mode=='ponder250-hit100':
                    send('go ponder movetime 100');time.sleep(.25)
                    started=time.monotonic();send('ponderhit')
                else:
                    started=time.monotonic();send('go depth 6' if 'depth6' in mode else 'go movetime 100')
                rows=wait('bestmove ');elapsed=(time.monotonic()-started)*1000
                info=next(x for x in rows if x.startswith('info depth '))
                fields={k:int(re.search(r'\b'+k+r' (-?\d+)',info).group(1)) for k in ['depth','nodes']}
                score=re.search(r'\bscore (cp|mate) (-?\d+)',info)
                writer.writerow([name,mode,repeat,fields['depth'],fields['nodes'],score.group(1),int(score.group(2)),rows[-1].split()[1],round(elapsed,2),250 if 'ponder250' in mode else 0])
                send('quit');p.wait(timeout=3)
            finally:
                if p.poll() is None:p.kill();p.wait()
