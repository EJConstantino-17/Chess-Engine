#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"

// pass the same state between turns; initialize from FEN in main.
void play_game_loop_with_state(Bitboard pieces[12], Bitboard occupancy[3],
                                int side_to_move, int ep_square, int castle_rights,
                                int engine_side, int search_depth, SearchState *history);

// Runs an interactive terminal game: alternates between the engine
// (playing `engine_side`, i.e. WHITE or BLACK) and a human typing UCI
// moves ("e2e4", "e7e8q", ...), until checkmate, stalemate, or the
// human types "quit". `search_depth` is passed straight through to
// search_best_move() for the engine's turns.
void play_game_loop(Bitboard pieces[12], Bitboard occupancy[3],
                     int side_to_move, int ep_square, int castle_rights,
                     int engine_side, int search_depth);

// Resolves a UCI move string against the actual legal moves in this
// position, so the result already carries the correct capture /
// en-passant / castle / promotion flags and can be passed straight to
// make_move(). Returns 1 and writes *out_move on a match, 0 if the
// string is malformed or doesn't match any legal move here.
int uci_to_move(const char *uci, Bitboard pieces[12], Bitboard occupancy[3],
                int side_to_move, int ep_square, int castle_rights,
                Move *out_move);

#endif
