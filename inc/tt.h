#ifndef TT_H
#define TT_H

#include "bitboard.h"
#include "movegen.h"
#include <stddef.h>

#define EXACT_BOUND 1
#define LOWER_BOUND 2
#define UPPER_BOUND 3

typedef struct {
    uint64_t key;
    Move move;
    int16_t score;
    int16_t depth;
    uint8_t flag;
} TT_Entry;

// Shared random keys for incremental position updates.
extern uint64_t z_pieces[12][64], z_side, z_castle[16], z_enpassant[64];
uint64_t zobrist_ep_key(Bitboard pieces[12], int side, int ep_square);
void init_zobrist(void);
uint64_t generate_zobrist_key(Bitboard pieces[12], int side_to_move, int ep_square, int castle_rights);

void init_tt(size_t size_in_mb);
void clear_tt(void);
void free_tt(void);
int tt_is_initialized(void);

// key_hits counts matching positions; score_hits counts usable cutoffs.
typedef struct {
    uint64_t probes;
    uint64_t key_hits;
    uint64_t score_hits;
    uint64_t shallow_hits;
    uint64_t bound_misses;
} TT_Stats;
void reset_tt_stats(void);
TT_Stats get_tt_stats(void);
Move lookup_tt_move(uint64_t key);
// candidate only; search verifies the move independently before extension.
Move tt_singular_candidate(uint64_t key, int minimum_depth);
void prefetch_tt(uint64_t key);

int read_tt(uint64_t key, int depth, int alpha, int beta, Move *tt_move, int *tt_score, int ply);
void write_tt(uint64_t key, Move best_move, int score, int depth, int flag, int ply);

#endif
