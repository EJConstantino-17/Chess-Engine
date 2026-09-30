#include "../nnue/cce_nnue.h"
#include "../inc/eval.h"
#include "../inc/movegen.h"
#include "../inc/search.h"
#include "../inc/tt.h"
#include <string.h>
#include <stdio.h>
#include "../inc/timing.h"

static Bitboard nodes_searched = 0;
// options can be toggled independently for diagnostics.
static SearchOptions options = {1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1};
static SearchStats stats;
static double search_start_ms;
static int aborted;
// prohibit recursive verification inside a verification search.
static int singular_verifying;
static int (*stop_hook)(void *);
static void *stop_context;
void search_set_stop_hook(int (*hook)(void *), void *context) {
    stop_hook = hook; stop_context = context;
}

void search_set_options(SearchOptions value) { options = value; }
SearchOptions search_get_options(void) { return options; }
SearchStats search_get_stats(void) { return stats; }

static int search_stop_requested(void) {
    if (aborted) return 1;
    if (options.max_nodes && nodes_searched >= options.max_nodes) aborted = 1;
    if (!aborted && stop_hook && (nodes_searched & 1023ULL) == 0 &&
        stop_hook(stop_context)) aborted = 1;
    // A scalar network must not overrun short budgets by 1024 expensive nodes.
    if (options.max_time_ms && (nodes_searched & 63ULL) == 0 &&
        cce_now_ms() - search_start_ms >= options.max_time_ms)
        aborted = 1;
    return aborted;
}

#define HISTORY_LIMIT 100000
static Move killer_moves[2][MAX_PLY];
static int history_moves[2][64][64];
static Move pv_table[MAX_PLY][MAX_PLY];
static int pv_length[MAX_PLY];
static Move previous_pv[MAX_PLY];
static int previous_pv_length;
static int in_check(Bitboard pieces[12], Bitboard occupancy[3], int side);

// root game history plus the current search path. Search is single-threaded.
static SearchState game_state;
static uint64_t path_keys[MAX_PLY];

void search_state_reset(SearchState *state, Bitboard pieces[12], int side_to_move,
                        int ep_square, int castle_rights, int halfmove_clock) {
    state->count = 1;
    state->halfmove_clock = halfmove_clock < 0 ? 0 : halfmove_clock;
    state->keys[0] = generate_zobrist_key(pieces, side_to_move, ep_square, castle_rights);
}

void search_state_record_move(SearchState *state, Move move, Bitboard pieces[12],
                              int side_to_move, int ep_square, int castle_rights) {
    // Call after a successful make_move; MOVE_PIECE still identifies the mover.
    if (MOVE_PIECE(move) == P || MOVE_PIECE(move) == p || MOVE_IS_CAPTURE(move))
        state->halfmove_clock = 0;
    else if (state->halfmove_clock < 10000) state->halfmove_clock++;
    if (state->count == SEARCH_HISTORY_CAPACITY) {
        memmove(state->keys, state->keys + 1, (SEARCH_HISTORY_CAPACITY - 1) * sizeof(uint64_t));
        state->count--;
    }
    state->keys[state->count++] = generate_zobrist_key(pieces, side_to_move, ep_square, castle_rights);
}

static int legal_move_exists(Bitboard pieces[12], Bitboard occupancy[3],
                             int side, int ep, int castle, int ply) {
    MoveList moves;
    generate_all_moves(&moves, pieces, occupancy, side, ep, castle);
    for (int i = 0; i < moves.count; i++) {
        int next_side = side, next_ep = ep, next_castle = castle;
        int legal = make_move(moves.moves[i], pieces, occupancy,
                              &next_side, &next_ep, &next_castle, ply);
        // failed make already restores the board; legal make needs unmake.
        if (legal) {
            unmake_move(moves.moves[i], pieces, occupancy,
                        &next_side, &next_ep, &next_castle, ply);
            return 1;
        }
    }
    return 0;
}

static Move first_legal_move(Bitboard pieces[12], Bitboard occupancy[3],
                             int side, int ep, int castle) {
    // a very short clock still requires a legal bestmove if one exists.
    MoveList moves;
    generate_all_moves(&moves, pieces, occupancy, side, ep, castle);
    for (int i = 0; i < moves.count; ++i) {
        int next_side = side, next_ep = ep, next_castle = castle;
        int legal = make_move(moves.moves[i], pieces, occupancy,
                              &next_side, &next_ep, &next_castle, 0);
        if (legal) {
            unmake_move(moves.moves[i], pieces, occupancy,
                        &next_side, &next_ep, &next_castle, 0);
            return moves.moves[i];
        }
    }
    return 0;
}

static int draw_rule_applies(uint64_t key, int halfmove, int ply,
                             Bitboard pieces[12], Bitboard occupancy[3],
                             int side, int ep, int castle) {
    int count = 1, remaining = halfmove;
    for (int i = ply - 1; i >= 1 && remaining > 0; i--, remaining--)
        if (path_keys[i] == key) count++;
    // The game history includes the root at its last index; exclude it at ply 0.
    for (int i = game_state.count - 1 - (ply == 0); i >= 0 && remaining > 0; i--, remaining--)
        if (game_state.keys[i] == key) count++;
    if (halfmove < 100 && count < 3) return 0;
    // Checkmate takes precedence over a claimable draw.
    if (in_check(pieces, occupancy, side) &&
        !legal_move_exists(pieces, occupancy, side, ep, castle, ply)) return 0;
    return 1;
}

static uint64_t context_tt_key(uint64_t key, int halfmove, int ply) {
    if (!options.tt_score_cutoffs) return key;
    // TT results depend on both the halfmove clock and prior positions.
    // Include the reversible path to prevent reusing a score from another history.
    uint64_t h = key ^ 0x9e3779b97f4a7c15ULL ^ (uint64_t)halfmove;
    int remaining = halfmove;
    for (int i = ply - 1; i >= 1 && remaining > 0; i--, remaining--)
        h = (h ^ path_keys[i]) * 0x100000001b3ULL;
    for (int i = game_state.count - 1 - (ply == 0); i >= 0 && remaining > 0; i--, remaining--)
        h = (h ^ game_state.keys[i]) * 0x100000001b3ULL;
    return h;
}

int search_draw_claim_available(Bitboard pieces[12], Bitboard occupancy[3],
                                 int side_to_move, int ep_square, int castle_rights,
                                 const SearchState *state) {
    uint64_t key = generate_zobrist_key(pieces, side_to_move, ep_square, castle_rights);
    if (state && state->count > 0 && state->count <= SEARCH_HISTORY_CAPACITY &&
        state->keys[state->count - 1] == key) game_state = *state;
    else search_state_reset(&game_state, pieces, side_to_move, ep_square, castle_rights,
                            state ? state->halfmove_clock : 0);
    path_keys[0] = key;
    return draw_rule_applies(key, game_state.halfmove_clock, 0, pieces, occupancy,
                             side_to_move, ep_square, castle_rights);
}

void clear_search_heuristics(void) {
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));
    memset(previous_pv, 0, sizeof(previous_pv));
    previous_pv_length = 0;
}

static int get_captured_piece(Move m, Bitboard pieces[12]) {
    if (MOVE_IS_EP(m)) {
        int moved_piece = MOVE_PIECE(m);
        return (moved_piece <= K) ? p : P;
    }

    int target_sq = MOVE_TARGET(m);
    for (int p_type = P; p_type <= k; p_type++) {
        if (pieces[p_type] & (1ULL << target_sq)) {
            return p_type;
        }
    }

    return NO_PIECE;
}

static inline int score_move(Move m, Bitboard pieces[12], Move pv_or_tt_move, int ply, int side_to_move) {

    if (pv_or_tt_move != 0 && m == pv_or_tt_move) {
        return 10000000;
    }

    if (MOVE_IS_CAPTURE(m)) {
        int attacker = MOVE_PIECE(m);
        int victim = get_captured_piece(m, pieces);

        if (victim != NO_PIECE) {
            int attacker_type = attacker % 6;
            int victim_type = victim % 6;

            int attacker_val = mg_value[attacker_type];
            int victim_val = mg_value[victim_type];

            if (victim_type == K) {
                victim_val = 20000;
            }

            return 1000000 + (victim_val * 10) - attacker_val;
        }

        return 1000000;
    }

    if (MOVE_PROMOTED(m)) {
        int promoted_type = MOVE_PROMOTED(m) % 6;
        return 900000 + mg_value[promoted_type];
    }

    if (ply < MAX_PLY) {
        if (m == killer_moves[0][ply]) return 500000;
        if (m == killer_moves[1][ply]) return 450000;
    }

    int hist = history_moves[side_to_move][MOVE_SRC(m)][MOVE_TARGET(m)];
    return (int)((int64_t)hist * 400000 / HISTORY_LIMIT); 
}

static int in_check(Bitboard pieces[12], Bitboard occupancy[3], int side) {
    int king_sq = get_lsb_index(pieces[side == WHITE ? K : k]);
    return king_sq >= 0 && is_square_attacked(king_sq, pieces, occupancy, side ^ 1);
}

static void pick_next_move(MoveList *move_list, int move_index, int move_scores[]) {
    int best_score = move_scores[move_index];
    int best_index = move_index;

    for (int i = move_index + 1; i < move_list->count; i++) {
        if (move_scores[i] > best_score) {
            best_score = move_scores[i];
            best_index = i;
        }
    }

    Move temp_move = move_list->moves[move_index];
    move_list->moves[move_index] = move_list->moves[best_index];
    move_list->moves[best_index] = temp_move;

    int temp_score = move_scores[move_index];
    move_scores[move_index] = move_scores[best_index];
    move_scores[best_index] = temp_score;
}

// One live metadata record follows the existing single-threaded search.
static MoveState search_position;

// Probes and verification searches share the deferred legality path.
static int search_make_legal(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                             int *side, int *ep, int *castle, int ply, MoveState *state) {
    if (!make_move_unchecked_state(move, pieces, occupancy, side, ep, castle, ply, state)) return 0;
    if (!move_is_legal_after_make(move, pieces, occupancy)) {
        unmake_move_state(move, pieces, occupancy, side, ep, castle, ply, state);
        return 0;
    }
    cce_nnue_commit(move, ply);
    return 1;
}

static int quiescence(int alpha, int beta, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int ply, int halfmove, int null_active) {
    nodes_searched++;
    stats.qnodes++;
    if (search_stop_requested()) return 0;
    uint64_t position_key = search_position.zobrist_key;
    path_keys[ply] = position_key;
    if (!null_active && draw_rule_applies(position_key, halfmove, ply, pieces, occupancy,
                          side_to_move, ep_square, castle_rights)) return 0;
    int checked = in_check(pieces, occupancy, side_to_move);

    if (ply >= MAX_PLY - 1) {
        if (checked) {
            MoveList evasions;
            generate_pseudo_legal_moves(&evasions, pieces, occupancy, side_to_move, ep_square, castle_rights);
            int best = -INFINITY_SCORE;
            for (int i = 0; i < evasions.count; i++) {
                Move m = evasions.moves[i];
                if (search_make_legal(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position)) {
                    int score = -evaluate(pieces, occupancy, side_to_move);
                    unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position);
                    if (score > best) best = score;
                }
            }
            return best == -INFINITY_SCORE ? -MATE_SCORE + ply : best;
        }
        return evaluate(pieces, occupancy, side_to_move);
    }

    if (!checked) {
        int stand_pat = evaluate(pieces, occupancy, side_to_move);

        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    }

    MoveList pseudo_moves;
    // In check, quiet evasions are still required.
    if (checked) generate_pseudo_legal_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);
    else generate_tactical_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square);

    int move_scores[sizeof(pseudo_moves.moves) / sizeof(pseudo_moves.moves[0])];
    int legal_moves_count = 0;
    for (int i = 0; i < pseudo_moves.count; i++) {
        move_scores[i] = score_move(pseudo_moves.moves[i], pieces, 0, ply, side_to_move);
    }

    for (int i = 0; i < pseudo_moves.count; i++) {

        pick_next_move(&pseudo_moves, i, move_scores);
        Move m = pseudo_moves.moves[i];

        if (!checked && !MOVE_IS_CAPTURE(m) && !MOVE_PROMOTED(m)) continue;
        int next_halfmove = (MOVE_PIECE(m) == P || MOVE_PIECE(m) == p || MOVE_IS_CAPTURE(m))
                            ? 0 : halfmove + 1;

        // Apply only the next ordered candidate; unplayed moves get no legality test.
        if (!make_move_unchecked_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position))
            continue;
        // Reject self-check before counting legal moves or entering a child.
        if (!move_is_legal_after_make(m, pieces, occupancy)) {
            unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position);
            continue;
        }
        cce_nnue_commit(m, ply);
        legal_moves_count++;

        int score = -quiescence(-beta, -alpha, pieces, occupancy, side_to_move, ep_square, castle_rights, ply + 1, next_halfmove, null_active);

        unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position);

        if (aborted) return 0;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    if (checked && legal_moves_count == 0) return -MATE_SCORE + ply;
    return alpha;
}

static int has_nonpawn_material(Bitboard pieces[12], int side) {
    return side == WHITE ? (pieces[N] | pieces[B] | pieces[R] | pieces[Q]) != 0
                         : (pieces[n] | pieces[b] | pieces[r] | pieces[q]) != 0;
}

// run independent, shallow searches of the candidate and its rivals.
// This avoids interpreting a canonical TT score as a history-safe score. A
// rival within 75 cp refutes uniqueness. Verification is bounded by search
// time/node limits and never writes TT entries for its artificial horizon.
static int negamax(int alpha, int beta, int depth, Bitboard pieces[12],
                   Bitboard occupancy[3], int side_to_move, int ep_square,
                   int castle_rights, int ply, int follow_pv, int halfmove,
                   int pv_node, int null_active);
static int verify_singular(MoveList *moves, Move candidate, int depth,
                           Bitboard pieces[12], Bitboard occupancy[3],
                           int side, int ep, int castle, int ply, int halfmove) {
    int verify_depth = depth - 6;
    int candidate_score = -INFINITY_SCORE;
    int found = 0;
    singular_verifying = 1;
    for (int pass = 0; pass < 2 && !aborted; pass++) {
        for (int i = 0; i < moves->count && !aborted; i++) {
            Move m = moves->moves[i];
            if ((m == candidate) != (pass == 0)) continue;
            int next_side = side, next_ep = ep, next_castle = castle;
            int next_halfmove = (MOVE_PIECE(m) == P || MOVE_PIECE(m) == p || MOVE_IS_CAPTURE(m))
                                ? 0 : halfmove + 1;
            if (!search_make_legal(m, pieces, occupancy, &next_side, &next_ep, &next_castle, ply, &search_position)) continue;
            int score;
            if (pass == 0) {
                found = 1;
                score = -negamax(-INFINITY_SCORE, INFINITY_SCORE, verify_depth,
                                 pieces, occupancy, next_side, next_ep, next_castle,
                                 ply + 1, 0, next_halfmove, 0, 0);
                candidate_score = score;
            } else {
                int threshold = candidate_score - 75;
                score = -negamax(-threshold, -threshold + 1, verify_depth,
                                 pieces, occupancy, next_side, next_ep, next_castle,
                                 ply + 1, 0, next_halfmove, 0, 0);
            }
            unmake_move_state(m, pieces, occupancy, &next_side, &next_ep, &next_castle, ply, &search_position);
            if (aborted) break;
            if (pass == 0 && (candidate_score > MATE_SCORE - MAX_PLY ||
                              candidate_score < -MATE_SCORE + MAX_PLY)) {
                found = 0; break;
            }
            if (pass == 1 && score >= candidate_score - 75) {
                found = 0;
                stats.singular_refutations++;
                break;
            }
        }
        if (!found) break;
    }
    singular_verifying = 0;
    return found && !aborted;
}

// full window for the first legal move; later moves get a scout window.
// NMP/LMR: selective searches are disabled along artificial null-move branches.
static int negamax(int alpha, int beta, int depth, Bitboard pieces[12],
                   Bitboard occupancy[3], int side_to_move, int ep_square,
                   int castle_rights, int ply, int follow_pv, int halfmove,
                   int pv_node, int null_active) {
    pv_length[ply] = ply;
    if (depth <= 0 || ply >= MAX_PLY - 1)
        return quiescence(alpha, beta, pieces, occupancy, side_to_move,
                          ep_square, castle_rights, ply, halfmove, null_active);

    nodes_searched++;
    if (search_stop_requested()) return 0;
    uint64_t position_key = search_position.zobrist_key;
    path_keys[ply] = position_key;
    uint64_t hash_key = context_tt_key(position_key, halfmove, ply);
    // overlap cache lookup with draw and check work.
    if (!null_active && options.tt_prefetch && tt_is_initialized()) {
        prefetch_tt(hash_key);
        stats.prefetches++;
    }
    if (!null_active && draw_rule_applies(position_key, halfmove, ply, pieces,
                                           occupancy, side_to_move, ep_square, castle_rights)) return 0;
    int checked = in_check(pieces, occupancy, side_to_move);
    Move tt_move = 0;
    int tt_score = 0;
    if (!null_active) {
        if (options.tt_score_cutoffs && !singular_verifying) {
            if (read_tt(hash_key, depth, alpha, beta, &tt_move, &tt_score, ply))
                return tt_score;
        } else tt_move = lookup_tt_move(hash_key);
    }

    MoveList moves;
    generate_pseudo_legal_moves(&moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    // avoid PV/check/null paths, near-draw positions,
    // and mate score windows. Never treat stalemate as a static win.
    int static_ok = !singular_verifying && !pv_node && !checked && !null_active && halfmove < 80 &&
                    alpha > -MATE_SCORE + MAX_PLY && beta < MATE_SCORE - MAX_PLY;
    int static_eval = 0;
    if (static_ok && (options.futility || options.reverse_futility || options.razoring))
        static_eval = evaluate(pieces, occupancy, side_to_move);
    if (static_ok && options.reverse_futility && depth <= 3 &&
        has_nonpawn_material(pieces, side_to_move) &&
        static_eval - (100 + 300 * depth) >= beta &&
        legal_move_exists(pieces, occupancy, side_to_move, ep_square, castle_rights, ply)) {
        stats.reverse_futility_cutoffs++;
        return beta; // fail-hard lower bound; do not write an EXACT TT entry
    }
    if (static_ok && options.razoring && depth <= 2 &&
        static_eval + (250 + 150 * depth) <= alpha &&
        legal_move_exists(pieces, occupancy, side_to_move, ep_square, castle_rights, ply)) {
        stats.razor_attempts++;
        int qscore = quiescence(alpha, beta, pieces, occupancy, side_to_move,
                                ep_square, castle_rights, ply, halfmove, null_active);
        if (aborted) return 0;
        if (qscore <= alpha) {
            stats.razor_cutoffs++;
            return alpha; // only a fail-low bound; qsearch found no rescue
        }
    }

    // tested after making a move so checks and legal-count semantics
    // remain correct. Pawn pushes and promotions are exempt tactical threats.
    int futility_node = static_ok && options.futility && depth <= 2 &&
                        static_eval + 175 * depth <= alpha;

    // passing is a bound only in a non-PV, non-check position with
    // non-pawn material. No null move is added to repetition history or the TT.
    if (options.null_move && !singular_verifying && !null_active && !pv_node && !checked && depth >= 5 &&
        halfmove < 90 && beta < MATE_SCORE - MAX_PLY &&
        has_nonpawn_material(pieces, side_to_move) &&
        evaluate(pieces, occupancy, side_to_move) >= beta &&
        legal_move_exists(pieces, occupancy, side_to_move, ep_square, castle_rights, ply)) {
        stats.null_attempts++;
        int reduction = 2 + depth / 4;
        int null_depth = depth - 1 - reduction;
        if (null_depth < 0) null_depth = 0;
        // Artificial null moves also advance and restore NNUE state.
        cce_nnue_null(pieces, side_to_move, ep_square, castle_rights);
        MoveState saved_position = search_position;
        search_position.zobrist_key ^= z_side ^ zobrist_ep_key(pieces, side_to_move, ep_square);
        if (search_position.halfmove_clock < UINT32_MAX) ++search_position.halfmove_clock;
        int score = -negamax(-beta, -beta + 1, null_depth, pieces, occupancy,
                              side_to_move ^ 1, NO_SQUARE, castle_rights,
                              ply + 1, 0, halfmove + 1, 0, 1);
        cce_nnue_undo_null();
        search_position = saved_position;
        if (aborted) return 0;
        if (score >= beta && score < MATE_SCORE - MAX_PLY) {
            stats.null_cutoffs++;
            return beta;
        }
    }

    // verify a sufficiently deep TT candidate at depth-6 before
    // adding exactly one ply. Skip checks, PV nodes, near-draw and null paths.
    Move singular_move = 0;
    if (options.singular && !singular_verifying && !null_active && !pv_node &&
        !checked && depth >= 10 && ply + depth + 1 < MAX_PLY && halfmove < 80 &&
        alpha > -MATE_SCORE + MAX_PLY && beta < MATE_SCORE - MAX_PLY) {
        Move candidate = tt_singular_candidate(hash_key, depth - 1);
        if (candidate) {
            stats.singular_attempts++;
            if (verify_singular(&moves, candidate, depth, pieces, occupancy,
                                side_to_move, ep_square, castle_rights, ply, halfmove)) {
                singular_move = candidate;
                stats.singular_extensions++;
            }
            // Verification searches overwrite PV scratch space at this ply.
            pv_length[ply] = ply;
            if (aborted) return 0;
        }
    }
    int move_scores[sizeof(moves.moves) / sizeof(moves.moves[0])];
    Move ordered = tt_move;
    if (!ordered && follow_pv && ply < previous_pv_length) ordered = previous_pv[ply];
    for (int i = 0; i < moves.count; i++)
        move_scores[i] = score_move(moves.moves[i], pieces, ordered, ply, side_to_move);

    int legal_count = 0;
    int original_alpha = alpha;
    Move best_move = 0;
    for (int i = 0; i < moves.count; i++) {
        pick_next_move(&moves, i, move_scores);
        Move m = moves.moves[i];
        int next_halfmove = MOVE_PIECE(m) == P || MOVE_PIECE(m) == p || MOVE_IS_CAPTURE(m)
                            ? 0 : halfmove + 1;
        // the current ply's undo record survives child search at ply + 1.
        if (!make_move_unchecked_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position))
            continue;
        // Reject self-check before counting legal moves or entering a child.
        if (!move_is_legal_after_make(m, pieces, occupancy)) {
            unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position);
            continue;
        }
        cce_nnue_commit(m, ply);
        legal_count++;
        int child_depth = depth - 1 + (m == singular_move);
        int child_pv = pv_node && legal_count == 1;
        int child_follow = follow_pv && ply < previous_pv_length && m == previous_pv[ply];
        int quiet = !MOVE_IS_CAPTURE(m) && !MOVE_PROMOTED(m);
        if (futility_node && quiet && MOVE_PIECE(m) != P && MOVE_PIECE(m) != p &&
            !in_check(pieces, occupancy, side_to_move)) {
            stats.futility_skips++;
            unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position);
            continue;
        }
        // preserve short tactical lines; depth-three reductions can
        // change the best move even when the reduced line does not raise alpha.
        int reduced = options.lmr && !singular_verifying && !null_active && depth >= 4 && legal_count >= 5 &&
                      !pv_node && !checked && quiet && m != ordered &&
                      !in_check(pieces, occupancy, side_to_move);
        int score = 0; // definite value when an aborted child skips re-search.
        if (reduced) {
            stats.lmr_candidates++;
            int reduction = 1 + (depth >= 6 && legal_count >= 8) +
                            (depth >= 10 && legal_count >= 15);
            if (reduction > child_depth - 1) reduction = child_depth - 1;
            score = -negamax(-alpha - 1, -alpha, child_depth - reduction, pieces,
                             occupancy, side_to_move, ep_square, castle_rights,
                             ply + 1, 0, next_halfmove, 0, null_active);
            if (!aborted && score > alpha) {
                stats.lmr_researches++;
                reduced = 0; // verify a promising reduced move at full depth
            }
            if (!aborted && reduced) stats.lmr_reductions++;
        }
        if (!reduced && !aborted) {
            if (!options.pvs || legal_count == 1) {
                score = -negamax(-beta, -alpha, child_depth, pieces, occupancy,
                                 side_to_move, ep_square, castle_rights, ply + 1,
                                 child_follow, next_halfmove, child_pv, null_active);
            } else {
                score = -negamax(-alpha - 1, -alpha, child_depth, pieces, occupancy,
                                 side_to_move, ep_square, castle_rights, ply + 1,
                                 child_follow, next_halfmove, 0, null_active);
                if (!aborted && score > alpha && score < beta) {
                    stats.pvs_researches++;
                    score = -negamax(-beta, -alpha, child_depth, pieces, occupancy,
                                     side_to_move, ep_square, castle_rights, ply + 1,
                                     child_follow, next_halfmove, pv_node, null_active);
                }
            }
        }
        unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply, &search_position);
        if (aborted) return 0;

        if (!best_move || score > alpha) best_move = m;
        if (score >= beta) {
            if (!null_active && !singular_verifying) write_tt(hash_key, m, score, depth, LOWER_BOUND, ply);
            if (quiet && !null_active) {
                if (killer_moves[0][ply] != m) {
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = m;
                }
                int *history = &history_moves[side_to_move][MOVE_SRC(m)][MOVE_TARGET(m)];
                int bonus = depth * depth;
                if (bonus > HISTORY_LIMIT) bonus = HISTORY_LIMIT;
                *history += bonus - (*history * bonus) / HISTORY_LIMIT;
            }
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            pv_table[ply][ply] = m;
            for (int j = ply + 1; j < pv_length[ply + 1]; j++)
                pv_table[ply][j] = pv_table[ply + 1][j];
            pv_length[ply] = pv_length[ply + 1];
        }
    }
    if (!legal_count) {
        int result = checked ? -MATE_SCORE + ply : 0;
        if (!null_active && !singular_verifying) write_tt(hash_key, 0, result, depth, EXACT_BOUND, ply);
        return result;
    }
    if (!null_active && !singular_verifying) write_tt(hash_key, best_move, alpha, depth,
                               alpha > original_alpha ? EXACT_BOUND : UPPER_BOUND, ply);
    return alpha;
}

typedef struct { Move move; int score; int legal; } RootResult;

static RootResult search_root(int alpha, int beta, int depth,
                              Bitboard pieces[12], Bitboard occupancy[3],
                              int side_to_move, int ep_square, int castle_rights,
                              Move preferred) {
    RootResult result = {0, -INFINITY_SCORE, 0};
    pv_length[0] = 0;
    MoveList moves;
    generate_pseudo_legal_moves(&moves, pieces, occupancy, side_to_move, ep_square, castle_rights);
    int move_scores[sizeof(moves.moves) / sizeof(moves.moves[0])];
    for (int i = 0; i < moves.count; i++)
        move_scores[i] = score_move(moves.moves[i], pieces, preferred, 0, side_to_move);
    for (int i = 0; i < moves.count; i++) {
        pick_next_move(&moves, i, move_scores);
        Move m = moves.moves[i];
        int next_halfmove = MOVE_PIECE(m) == P || MOVE_PIECE(m) == p || MOVE_IS_CAPTURE(m)
                            ? 0 : game_state.halfmove_clock + 1;
        // do not copy the 12 piece bitboards for every candidate.
        if (!make_move_unchecked_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, 0, &search_position))
            continue;
        // Reject self-check before counting legal moves or entering a child.
        if (!move_is_legal_after_make(m, pieces, occupancy)) {
            unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, 0, &search_position);
            continue;
        }
        cce_nnue_commit(m, 0);
        result.legal++;
        int score;
        int child_follow = previous_pv_length > 0 && m == previous_pv[0];
        if (!options.pvs || result.legal == 1) {
            score = -negamax(-beta, -alpha, depth - 1, pieces, occupancy,
                             side_to_move, ep_square, castle_rights, 1,
                             child_follow, next_halfmove, 1, 0);
        } else {
            score = -negamax(-alpha - 1, -alpha, depth - 1, pieces, occupancy,
                             side_to_move, ep_square, castle_rights, 1,
                             child_follow, next_halfmove, 0, 0);
            if (!aborted && score > alpha && score < beta) {
                stats.pvs_researches++;
                score = -negamax(-beta, -alpha, depth - 1, pieces, occupancy,
                                 side_to_move, ep_square, castle_rights, 1,
                                 child_follow, next_halfmove, 1, 0);
            }
        }
        unmake_move_state(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, 0, &search_position);
        if (aborted) return result;
        if (score > result.score) { result.score = score; result.move = m; }
        if (score > alpha) {
            alpha = score;
            pv_table[0][0] = m;
            for (int j = 1; j < pv_length[1]; j++) pv_table[0][j] = pv_table[1][j];
            pv_length[0] = pv_length[1];
        }
        if (score >= beta) break;
    }
    if (!result.legal) result.score = in_check(pieces, occupancy, side_to_move) ? -MATE_SCORE : 0;
    return result;
}

Move search_best_move_with_state(Bitboard pieces[12], Bitboard occupancy[3],
                                 int side_to_move, int ep_square, int castle_rights,
                                 int max_depth, const SearchState *state) {
    nodes_searched = 0;
    memset(&stats, 0, sizeof(stats));
    aborted = 0;
    singular_verifying = 0;
    search_start_ms = cce_now_ms();
    clear_search_heuristics();
    if (!tt_is_initialized()) init_tt(16);
    reset_tt_stats();
    if (search_draw_claim_available(pieces, occupancy, side_to_move, ep_square,
                                    castle_rights, state) && options.claim_draw) {
        if (options.verbose) puts("Draw claim available: threefold repetition or fifty-move rule.");
        stats.elapsed_ms = cce_now_ms() - search_start_ms;
        return 0;
    }
    move_state_init(&search_position, pieces, side_to_move, ep_square, castle_rights,
                    (uint32_t)game_state.halfmove_clock);
    Move best_move = 0;
    int previous_score = 0;
    for (int depth = 1; depth <= max_depth && depth < MAX_PLY; depth++) {
        TT_Stats before = get_tt_stats();
        int delta = options.aspiration && depth >= 2 ? 35 : INFINITY_SCORE;
        RootResult result = {0, -INFINITY_SCORE, 0};
        while (!aborted) {
            int alpha = delta >= INFINITY_SCORE ? -INFINITY_SCORE : previous_score - delta;
            int beta  = delta >= INFINITY_SCORE ?  INFINITY_SCORE : previous_score + delta;
            if (alpha < -INFINITY_SCORE) alpha = -INFINITY_SCORE;
            if (beta > INFINITY_SCORE) beta = INFINITY_SCORE;
            result = search_root(alpha, beta, depth, pieces, occupancy,
                                 side_to_move, ep_square, castle_rights, best_move);
            if (aborted || !result.legal) break;
            if ((result.score <= alpha && alpha > -INFINITY_SCORE) ||
                (result.score >= beta && beta < INFINITY_SCORE)) {
                stats.aspiration_researches++;
                if (delta >= INFINITY_SCORE / 2) delta = INFINITY_SCORE;
                else delta *= 2;
                continue;
            }
            break;
        }
        if (aborted) { stats.stopped = 1; break; } // preserve last completed depth
        if (!result.legal) break;
        best_move = result.move;
        previous_score = result.score;
        stats.completed_depth = depth;
        stats.score = result.score;
        stats.best_move = best_move;
        previous_pv_length = pv_length[0];
        for (int j = 0; j < previous_pv_length; j++) previous_pv[j] = pv_table[0][j];
        stats.pv_count = previous_pv_length;
        for (int j = 0; j < stats.pv_count; j++) stats.pv[j] = previous_pv[j];
        if (options.verbose) {
            TT_Stats after = get_tt_stats();
            uint64_t probes = after.probes - before.probes;
            uint64_t hits = after.score_hits - before.score_hits;
            printf("Search depth: %d | Nodes searched: %" PRIu64 " | Best score: %d | "
                   "TT score hits: %.1f%% | aspiration retries: %" PRIu64 "\n",
                   depth, nodes_searched, result.score,
                   probes ? 100.0 * hits / probes : 0.0, stats.aspiration_researches);
        }
        // quiescence may spot a mate beyond this iteration.
        // Keep deepening until the nominal depth reaches its reported mate ply;
        // otherwise a mate in three can prevent discovering a mate in two.
        if ((result.score >= MATE_SCORE - MAX_PLY &&
             depth >= MATE_SCORE - result.score) ||
            (result.score <= -MATE_SCORE + MAX_PLY &&
             depth >= MATE_SCORE + result.score)) break;
    }
    stats.nodes = nodes_searched;
    TT_Stats tt_final = get_tt_stats();
    stats.tt_probes = tt_final.probes;
    stats.tt_key_hits = tt_final.key_hits;
    stats.tt_score_hits = tt_final.score_hits;
    if (!best_move && aborted)
        best_move = first_legal_move(pieces, occupancy, side_to_move,
                                     ep_square, castle_rights);
    stats.best_move = best_move;
    if (options.verbose && stats.stopped)
        printf("Search stopped after depth %d (node/time limit)\n", stats.completed_depth);
    stats.elapsed_ms = cce_now_ms() - search_start_ms;
    return best_move;
}

// Legacy entry point starts with no known prior moves (halfmove 0).
Move search_best_move(Bitboard pieces[12], Bitboard occupancy[3], int side_to_move,
                       int ep_square, int castle_rights, int max_depth) {
    return search_best_move_with_state(pieces, occupancy, side_to_move,
                                       ep_square, castle_rights, max_depth, NULL);
}
