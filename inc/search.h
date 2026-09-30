#ifndef SEARCH_H
#define SEARCH_H

#include "movegen.h"
#include <stdint.h>

#define INFINITY_SCORE 30000
#define MATE_SCORE 29000

// Keep actual game history across calls to the search.
#define SEARCH_HISTORY_CAPACITY 256
typedef struct {
    uint64_t keys[SEARCH_HISTORY_CAPACITY]; // includes the current position as the last key
    int count;
    int halfmove_clock; // plies since last pawn move or capture (FEN field 5)
} SearchState;

void search_state_reset(SearchState *state, Bitboard pieces[12], int side_to_move,
                        int ep_square, int castle_rights, int halfmove_clock);
void search_state_record_move(SearchState *state, Move move, Bitboard pieces[12],
                              int side_to_move, int ep_square, int castle_rights);
int search_draw_claim_available(Bitboard pieces[12], Bitboard occupancy[3],
                                 int side_to_move, int ep_square, int castle_rights,
                                 const SearchState *state);
Move search_best_move_with_state(Bitboard pieces[12], Bitboard occupancy[3],
                                 int side_to_move, int ep_square, int castle_rights,
                                 int depth, const SearchState *state);

// switches make diagnostic comparisons reproducible.
typedef struct {
    int aspiration, pvs, null_move, lmr;
    int tt_score_cutoffs; // 0: safe move ordering only; 1: experimental score reuse
    uint64_t max_nodes;   // 0: no limit
    int max_time_ms;       // 0: no limit (elapsed wall time)
    int verbose;           // print per-depth search and TT statistics
    int claim_draw;        // interactive mode may claim; UCI always sends a move
    // switches permit identical-position A/B diagnostics.
    int futility, reverse_futility, razoring, tt_prefetch;
    int singular; // verified +1 ply extension for a uniquely strong TT move
} SearchOptions;
typedef struct {
    uint64_t nodes, qnodes, null_cutoffs, lmr_researches, pvs_researches;
    uint64_t aspiration_researches;
    uint64_t null_attempts, lmr_candidates, lmr_reductions;
    uint64_t tt_probes, tt_key_hits, tt_score_hits;
    uint64_t futility_skips, reverse_futility_cutoffs, razor_attempts, razor_cutoffs;
    uint64_t prefetches;
    uint64_t singular_attempts, singular_extensions, singular_refutations;
    double elapsed_ms; // Measured search duration for UCI time/NPS.
    int completed_depth, score, stopped;
    Move best_move;
    Move pv[MAX_PLY];
    int pv_count; // Last fully completed iteration; empty if no iteration completed.
} SearchStats;
void search_set_options(SearchOptions options);
SearchOptions search_get_options(void);
SearchStats search_get_stats(void);
// the input hook lets a single-threaded search react to stop/quit.
void search_set_stop_hook(int (*hook)(void *), void *context);

Move search_best_move(Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int depth);

#endif
