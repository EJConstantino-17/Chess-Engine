"""Convert Lichess openings TSV or standard-start PGN games to UCI book lines.

Usage: python tools/lichess_to_uci.py opening_book_large.txt a.tsv b.tsv games.pgn
Requires: python -m pip install chess  (for SAN TSV and PGN conversion).
The Lichess input TSV has eco/name/pgn (SAN); its generated dist TSV may
instead include uci, which this tool copies without requiring python-chess.
"""
import csv
import io
import sys
from pathlib import Path

MAX_BOOK_PLIES = 24  # matches tools/build_book.c's per-line limit.


def require_chess():
    try:
        import chess
        import chess.pgn
    except ImportError as exc:
        raise SystemExit('Install offline converter dependency: '
                         'python -m pip install chess') from exc
    return chess


def pgn_lines(stream, target):
    """PGN IMPORT: read one game at a time; ignore variants and custom starts."""
    chess = require_chess()
    accepted = rejected = 0
    while (game := chess.pgn.read_game(stream)) is not None:
        if game.errors or game.board().fen() != chess.STARTING_FEN:
            rejected += 1
            continue
        board = game.board()
        moves = []
        for move in game.mainline_moves():
            if len(moves) >= MAX_BOOK_PLIES:
                break
            moves.append(board.uci(move))
            board.push(move)
        if moves:
            target.write(' '.join(moves) + '\n')
            accepted += 1
        else:
            rejected += 1
    return accepted, rejected


def convert(output: Path, sources: list[Path]) -> None:
    imported = rejected = 0
    with output.open('w', encoding='utf-8', newline='\n') as target:
        target.write('# UCI opening lines generated from input TSV/PGN files.\n')
        for path in sources:
            if path.suffix.lower() == '.pgn':
                # stream large multi-game files without loading them all.
                with path.open('r', encoding='utf-8-sig', errors='replace') as stream:
                    count, bad = pgn_lines(stream, target)
                imported += count
                rejected += bad
                continue
            with path.open('r', encoding='utf-8', newline='') as stream:
                for row in csv.DictReader(stream, delimiter='\t'):
                    if row.get('uci'):
                        moves = row['uci'].strip().split()
                    elif row.get('pgn'):
                        chess = require_chess()
                        game = chess.pgn.read_game(io.StringIO(row['pgn']))
                        if game is None or game.errors:
                            rejected += 1
                            continue
                        board = game.board()
                        moves = []
                        for move in game.mainline_moves():
                            if len(moves) >= MAX_BOOK_PLIES:
                                break
                            moves.append(board.uci(move))
                            board.push(move)
                    else:
                        rejected += 1
                        continue
                    if moves and all(len(m) in (4, 5) for m in moves):
                        target.write(' '.join(moves) + '\n')
                        imported += 1
                    else:
                        rejected += 1
    print(f'converted={imported} rejected={rejected}; output={output}')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    convert(Path(sys.argv[1]), [Path(p) for p in sys.argv[2:]])
