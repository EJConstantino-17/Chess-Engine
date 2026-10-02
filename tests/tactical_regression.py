"""Run: python tests/tactical_regression.py ENGINE NETWORK (requires python-chess)."""
import os
from pathlib import Path
import sys

import chess
import chess.engine

MORPHY = "Q7/p1p1q1pk/3p2rp/4n3/3bP3/7b/PP3PPK/R1B2R2 b - - 0 1"
QUIESCENCE_PV = "r3k3/pbpqb1r1/1p2Q1p1/3pP1B1/3P4/3B4/PPP4P/5RK1 w - - 1 1"
env = {**os.environ, "CCE_NNUE_FILE": str(Path(sys.argv[2]).resolve())}
env.pop("CCE_EVAL", None)
with chess.engine.SimpleEngine.popen_uci(str(Path(sys.argv[1]).resolve()), env=env, timeout=30) as engine:
    assert engine.options["UseNNUE"].default, "NNUE must load successfully"
    engine.configure({"OwnBook": False})
    cases = [("Morphy", chess.Board(MORPHY), 7, "h3g2", 4),
             ("Quiescence PV", chess.Board(QUIESCENCE_PV), 7, "d3g6", 4)]
    child = chess.Board(MORPHY)
    child.push_uci("h3g2"); child.push_uci("f2f4")
    cases.append(("Qh4 immediate mate", child, 1, "e7h4", 1))
    for name, board, depth, expected, mate in cases:
        info = engine.analyse(board, chess.engine.Limit(depth=depth, time=10), game=object())
        assert info["pv"][0].uci() == expected, (name, info)
        assert info["score"].pov(board.turn).mate() == mate, (name, info)
        for move in info["pv"]:
            assert move in board.legal_moves, (name, move)
            board.push(move)
        assert board.is_checkmate(), (name, "PV does not end in mate", info)
        print("PASS:", name, "mate score, best move, complete legal PV")
    # Mate reporting must also preserve the losing side's sign.
    board = chess.Board("7k/8/5QK1/8/8/8/8/8 b - - 0 1")
    info = engine.analyse(board, chess.engine.Limit(depth=4, time=10), game=object())
    assert info["score"].pov(board.turn).mate() == -1, info
    print("PASS: losing mate uses negative UCI mate score")
print("4 tactical regressions passed")
