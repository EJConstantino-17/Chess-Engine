#include "tt.h"
#include "../inc/search.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

uint64_t z_pieces[12][64];
uint64_t z_side;
uint64_t z_castle[16];
uint64_t z_enpassant[64];

TT_Entry *tt_table = NULL;
size_t tt_size_entries = 0;
static int zobrist_ready = 0;
// counters are per search, independent of table contents.
static TT_Stats tt_stats;
void reset_tt_stats(void) { memset(&tt_stats, 0, sizeof(tt_stats)); }
TT_Stats get_tt_stats(void) { return tt_stats; }

static uint64_t rand64_seed = 1070372;
static uint64_t rand_64(void) {
    uint64_t x = rand64_seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rand64_seed = x;
    return x * 0x2545F4914F6CDD1DULL;
}

// An ep square matters for repetition only if a legal capture exists.
static int has_legal_ep(Bitboard pieces[12], int side, int ep_square) {
    if (ep_square < 0 || ep_square >= 64) return 0;
    int own_pawn = side == WHITE ? P : p;
    int their_pawn = side == WHITE ? p : P;
    int captured_sq = ep_square + (side == WHITE ? -8 : 8);
    if (captured_sq < 0 || captured_sq >= 64 ||
        !(pieces[their_pawn] & (1ULL << captured_sq))) return 0;
    Bitboard candidates = pawn_attacks[side ^ 1][ep_square] & pieces[own_pawn];
    while (candidates) {
        int from = pop_lsb(&candidates);
        Bitboard next[12];
        memcpy(next, pieces, sizeof(next));
        next[own_pawn] &= ~(1ULL << from);
        next[own_pawn] |= 1ULL << ep_square;
        next[their_pawn] &= ~(1ULL << captured_sq);
        Bitboard occupied[3] = {0, 0, 0};
        for (int piece = P; piece <= K; piece++) occupied[WHITE] |= next[piece];
        for (int piece = p; piece <= k; piece++) occupied[BLACK] |= next[piece];
        occupied[BOTH] = occupied[WHITE] | occupied[BLACK];
        int king_sq = get_lsb_index(next[side == WHITE ? K : k]);
        if (king_sq >= 0 && !is_square_attacked(king_sq, next, occupied, side ^ 1)) return 1;
    }
    return 0;
}

void init_zobrist(void) {
    
    clear_tt();
    rand64_seed = 1070372;

    for (int p = 0; p < 12; p++) {
        for (int sq = 0; sq < 64; sq++) {
            z_pieces[p][sq] = rand_64();
        }
    }

    z_side = rand_64();

    for (int i = 0; i < 16; i++) {
        z_castle[i] = rand_64();
    }

    for (int sq = 0; sq < 64; sq++) {
        z_enpassant[sq] = rand_64();
    }

    zobrist_ready = 1;
}

uint64_t generate_zobrist_key(Bitboard pieces[12], int side_to_move, int ep_square, int castle_rights) {
    if (!zobrist_ready) init_zobrist();
    uint64_t key = 0ULL;

    // compare the changing loop variable, not the constant black-pawn index `p`.
    for (int p_type = P; p_type <= k; p_type++) {
        Bitboard bb = pieces[p_type];
        while (bb) {
            int sq = pop_lsb(&bb);
            key ^= z_pieces[p_type][sq];
        }
    }

    if (side_to_move == BLACK) {
        key ^= z_side;
    }

    key ^= z_castle[castle_rights & 0x0F];

    // ignore phantom/illegal ep rights for position identity.
    if (has_legal_ep(pieces, side_to_move, ep_square)) {
        key ^= z_enpassant[ep_square];
    }

    return key;
}

void init_tt(size_t size_in_mb) {
    if (tt_table != NULL) free_tt();
    if (!zobrist_ready) init_zobrist();

    if (size_in_mb > SIZE_MAX / (1024 * 1024)) return;
    size_t bytes = size_in_mb * 1024 * 1024;
    tt_size_entries = bytes / sizeof(TT_Entry);
    if (tt_size_entries == 0) return;

    tt_table = (TT_Entry *)malloc(tt_size_entries * sizeof(TT_Entry));
    if (!tt_table) { tt_size_entries = 0; return; }
    clear_tt();
}

int tt_is_initialized(void) { return tt_table != NULL && tt_size_entries != 0; }

void prefetch_tt(uint64_t key) {
    // only hint a valid table entry; prefetch never dereferences it.
    // The caller overlaps this hint with draw/check work before the actual probe.
#if defined(__GNUC__) || defined(__clang__)
    if (tt_table && tt_size_entries)
        __builtin_prefetch(&tt_table[key % tt_size_entries], 0, 1);
#else
    (void)key;
#endif
}

Move lookup_tt_move(uint64_t key) {
    // TT moves aid ordering without trusting a path-dependent score.
    if (!tt_is_initialized()) return 0;
    tt_stats.probes++;
    TT_Entry *entry = &tt_table[key % tt_size_entries];
    if (entry->flag == 0 || entry->key != key) return 0;
    tt_stats.key_hits++;
    return entry->move;
}

// accept only an adequately deep exact/lower TT move as a hint.
// The stored score is path sensitive and is never trusted for verification.
Move tt_singular_candidate(uint64_t key, int minimum_depth) {
    if (!tt_is_initialized()) return 0;
    TT_Entry *entry = &tt_table[key % tt_size_entries];
    if (entry->key != key || !entry->move || entry->depth < minimum_depth ||
        (entry->flag != EXACT_BOUND && entry->flag != LOWER_BOUND)) return 0;
    return entry->move;
}

void clear_tt(void) {
    if (tt_table != NULL) {
        memset(tt_table, 0, tt_size_entries * sizeof(TT_Entry));
    }
}

void free_tt(void) {
    if (tt_table != NULL) {
        free(tt_table);
        tt_table = NULL;
    }
    tt_size_entries = 0;
}

int read_tt(uint64_t key, int depth, int alpha, int beta, Move *tt_move, int *tt_score, int ply) {
    if (!tt_is_initialized()) return 0;

    // a matching key can still have insufficient depth or a non-cutting bound.
    tt_stats.probes++;
    size_t index = key % tt_size_entries;
    TT_Entry *entry = &tt_table[index];

    if (entry->flag != 0 && entry->key == key) {
        tt_stats.key_hits++;
        *tt_move = entry->move;

        // only reuse a score searched to this exact depth.
        // Deeper entries remain useful as ordering hints, but substituting
        // their horizon-dependent score changed the selected puzzle line.
        if (entry->depth == depth) {
            int score = entry->score;

            if (score >= MATE_SCORE - MAX_PLY) score -= ply;
            if (score <= -MATE_SCORE + MAX_PLY) score += ply;
            
            if (entry->flag == EXACT_BOUND) {
                *tt_score = score;
                tt_stats.score_hits++;
                return 1;
            }

            if (entry->flag == LOWER_BOUND && score >= beta) {
                *tt_score = score;
                tt_stats.score_hits++;
                return 1;
            }

            if (entry->flag == UPPER_BOUND && score <= alpha) {
                *tt_score = score;
                tt_stats.score_hits++;
                return 1;
            }
            tt_stats.bound_misses++;
        } else {
            tt_stats.shallow_hits++;
        }
    }

    return 0;
}

void write_tt(uint64_t key, Move best_move, int score, int depth, int flag, int ply) {
    if (!tt_is_initialized()) return;

    size_t index = key % tt_size_entries;
    TT_Entry *entry = &tt_table[index];

    if (entry->flag != 0 && entry->key == key &&
        (entry->depth > depth || (entry->depth == depth && entry->flag == EXACT_BOUND && flag != EXACT_BOUND)))
        return;

    if (score >= MATE_SCORE - MAX_PLY) score += ply;
    if (score <= -MATE_SCORE + MAX_PLY) score -= ply;

    entry->key = key;
    entry->move = best_move;
    entry->score = (int16_t) score;
    entry->depth = (int16_t) depth;
    entry->flag = (int8_t) flag;
}
