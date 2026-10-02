"""Compare tactical search builds against Harvey's mate-in-four collection.
Requires python-chess: python -m pip install python-chess
"""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import time
from urllib.request import urlopen

import chess
import chess.engine


def load_positions(text):
    lines = text.splitlines()
    positions = []
    for index, fen in enumerate(lines):
        if not re.match(r"^[rnbqkpRNBQKP1-8/]+ [wb] ", fen):
            continue
        board = chess.Board(fen)
        solution = []
        # Some entries omit the space after the move number (e.g. 1.Nf7+).
        for token in re.sub(r"\d+\.+", " ", lines[index + 1]).split():
            move = board.parse_san(token)
            solution.append(move.uci())
            board.push(move)
        if not board.is_checkmate():
            raise ValueError(f"Dataset line {index + 1} does not end in mate")
        positions.append((fen, solution))
    if not positions:
        raise ValueError("No puzzles found")
    return positions


def run(executable, network, positions, depth, milliseconds, label):
    env = {**os.environ, "CCE_NNUE_FILE": str(network)}
    env.pop("CCE_EVAL", None)
    engine = chess.engine.SimpleEngine.popen_uci(str(executable), env=env, timeout=30)
    results = []
    try:
        if not engine.options.get("UseNNUE") or not engine.options["UseNNUE"].default:
            raise RuntimeError("Engine did not enable NNUE at startup")
        engine.configure({"OwnBook": False})
        for index, (fen, solution) in enumerate(positions):
            board = chess.Board(fen)
            started = time.monotonic()
            info = engine.analyse(board, chess.engine.Limit(depth=depth, time=milliseconds / 1000), game=object())
            elapsed = (time.monotonic() - started) * 1000
            pv = info.get("pv", [])
            score = info["score"].pov(board.turn)
            legal = True
            for move in pv:
                if move not in board.legal_moves:
                    legal = False
                    break
                board.push(move)
            # Older CCE builds encoded internal mate scores as centipawns.
            cp, mate = score.score(), score.mate()
            if mate is None and cp is not None and abs(cp) >= 28744:
                distance = (29000 - abs(cp) + 1) // 2
                mate = distance if cp >= 0 else -distance
            results.append({"build": label, "index": index, "fen": fen, "solution": solution,
                            "move": pv[0].uci() if pv else "0000", "cp": cp, "mate_score": mate,
                            "depth": info.get("depth", 0), "nodes": info.get("nodes", 0),
                            "elapsed_ms": round(elapsed, 3), "pv": [m.uci() for m in pv],
                            "legal": legal, "pv_ends_in_mate": board.is_checkmate(),
                            "first_move_match": bool(pv) and pv[0].uci() == solution[0]})
            if index % 50 == 0:
                print(f"{label}: {index}/{len(positions)}", flush=True)
    finally:
        engine.quit()
    return results


def summarize(rows):
    return {"positions": len(rows), "first_move_matches": sum(r["first_move_match"] for r in rows),
            "reported_mate_in_four_or_less": sum(r["mate_score"] is not None and 0 < r["mate_score"] <= 4 for r in rows),
            "complete_mating_pvs": sum(r["pv_ends_in_mate"] for r in rows),
            "illegal_pvs": sum(not r["legal"] for r in rows),
            "nodes": sum(r["nodes"] for r in rows),
            "elapsed_ms": round(sum(r["elapsed_ms"] for r in rows), 3)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--network", type=Path, required=True)
    parser.add_argument("--dataset", type=Path, help="Local copy of m8n4.txt; otherwise download it")
    parser.add_argument("--depth", type=int, default=8)
    parser.add_argument("--time-ms", type=int, default=200)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--output", type=Path, default=Path("benchmarks/tactical"))
    args = parser.parse_args()
    if not 1 <= args.depth < 256 or args.time_ms < 1 or args.limit < 0:
        parser.error("Depth must be 1..255; time-ms positive; limit nonnegative")
    if args.dataset:
        text = args.dataset.read_text(encoding="utf-8-sig")
    else:
        with urlopen("https://wtharvey.com/m8n4.txt", timeout=30) as response:
            text = response.read().decode("utf-8-sig")
    positions = load_positions(text)
    if args.limit:
        positions = positions[:args.limit]
    args.output.mkdir(parents=True, exist_ok=True)
    summary = {"depth_cap": args.depth, "budget_ms": args.time_ms,
               "source": "https://wtharvey.com/m8n4.txt", "builds": {}}
    for label, executable in (("before", args.before), ("after", args.after)):
        if executable is None:
            continue
        rows = run(executable.resolve(), args.network.resolve(), positions, args.depth, args.time_ms, label)
        (args.output / (label + ".json")).write_text(json.dumps(rows, indent=2), encoding="utf-8")
        with (args.output / (label + ".csv")).open("w", newline="", encoding="utf-8") as handle:
            columns = [k for k in rows[0] if k not in ("solution", "pv")]
            writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)
        summary["builds"][label] = summarize(rows)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    if any(b["illegal_pvs"] for b in summary["builds"].values()):
        raise SystemExit("Illegal principal variation")


if __name__ == "__main__":
    main()
