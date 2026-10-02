"""Run fixed-depth and timed TT/history ablations, checking deterministic searches."""
import argparse
import csv
import json
import os
from pathlib import Path
import statistics
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('harness', type=Path)
p.add_argument('network', type=Path)
p.add_argument('--output', type=Path, default=Path('benchmarks/tt_history_local'))
p.add_argument('--summarize-only', action='store_true')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
settings = [('depth8', 8, 0), ('depth10', 10, 0), ('time100', 30, 100), ('time200', 30, 200)]
modes = ['current', 'tt_only', 'history_only', 'history_lmr', 'combined']
summary = {}
for label, depth, ms in settings:
    runs = []
    for repetition in range(2):
        output = a.output / (label + ('_repeat' if repetition else '') + '.csv')
        if not a.summarize_only:
            with output.open('w', newline='') as stream:
                subprocess.run([str(a.harness.resolve()), str(a.network.resolve()), str(depth), str(ms), '--tt-history'],
                               stdout=stream, check=True,
                               env={**os.environ, 'CCE_NNUE_FILE': str(a.network.resolve())})
        with output.open(newline='') as stream:
            rows = list(csv.DictReader(stream))
        if len(rows) != 30 or {(r['mode'], r['case']) for r in rows} != {(m, str(i)) for m in modes for i in range(6)}:
            raise RuntimeError(f'Incomplete or duplicate rows: {output}')
        runs.append(rows)
    if not ms:
        deterministic = ['mode', 'case', 'depth', 'score', 'move', 'nodes', 'qnodes', 'tt_hits',
                         'history_bonuses', 'history_maluses', 'history_lmr_less', 'history_lmr_more', 'pv_count']
        for first, second in zip(*runs):
            if any(first[k] != second[k] for k in deterministic):
                raise RuntimeError(f'Non-deterministic fixed-depth result: {label}, {first["mode"]}, case {first["case"]}')
    summary[label] = {}
    for mode in modes:
        selected = [[r for r in run if r['mode'] == mode] for run in runs]
        complete = all(int(r['depth']) == depth for run in selected for r in run) if not ms else None
        nodes = statistics.mean(sum(int(r['nodes']) for r in run) for run in selected)
        elapsed = statistics.mean(sum(float(r['ms']) for r in run) for run in selected)
        record = {'all_fixed_depths_complete': complete, 'mean_total_nodes': nodes,
                  'mean_total_ms': elapsed, 'weighted_nps': nodes * 1000 / elapsed,
                  'depth_ranges': [[min(int(run[i]['depth']) for run in selected),
                                    max(int(run[i]['depth']) for run in selected)] for i in range(6)]}
        summary[label][mode] = record
        print(f'{label},{mode},nodes={nodes:.0f},ms={elapsed:.2f},depths={record["depth_ranges"]},complete={complete}')
(a.output / 'summary.json').write_text(json.dumps(summary, indent=2))
print('PASS: fixed-depth repeats are deterministic; timed results are summarized as ranges.')
