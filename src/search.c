#include "eval.h"
#include "movegen.h"
#include "../inc/search.h"
#include <string.h>
#include <stdio.h>

static Bitboard nodes_searched = 0;

#define HISTORY_LIMIT 100000
static Move killer_moves[2][MAX_PLY];
static int history_moves[2][64][64];
static Move pv_table[MAX_PLY][MAX_PLY];
static int pv_length[MAX_PLY];
static Move previous_pv[MAX_PLY];
static int previous_pv_length;

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

static int score_move(Move m, Bitboard pieces[12], Move pv_or_tt_move, int ply, int side_to_move) {

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
    return (hist * 400000) / HISTORY_LIMIT;
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

static int quiescence(int alpha, int beta, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int ply) {
    nodes_searched++;
    int checked = in_check(pieces, occupancy, side_to_move);

    if (ply >= MAX_PLY - 1) {
        if (checked) {
            MoveList evasions;
            generate_all_moves(&evasions, pieces, occupancy, side_to_move, ep_square, castle_rights);
            int best = -INFINITY_SCORE;
            for (int i = 0; i < evasions.count; i++) {
                Move m = evasions.moves[i];
                if (make_move(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply)) {
                    int score = -evaluate(pieces, occupancy, side_to_move);
                    unmake_move(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply);
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
    generate_all_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    int move_scores[sizeof(pseudo_moves.moves) / sizeof(pseudo_moves.moves[0])];
    int legal_moves_count = 0;
    for (int i = 0; i < pseudo_moves.count; i++) {
        move_scores[i] = score_move(pseudo_moves.moves[i], pieces, 0, ply, side_to_move);
    }

    for (int i = 0; i < pseudo_moves.count; i++) {

        pick_next_move(&pseudo_moves, i, move_scores);
        Move m = pseudo_moves.moves[i];

        if (!checked && !MOVE_IS_CAPTURE(m) && !MOVE_PROMOTED(m)) continue;

        Bitboard pieces_copy[12];
        Bitboard occ_copy[3];
        memcpy(pieces_copy, pieces, sizeof(pieces_copy));
        memcpy(occ_copy, occupancy, sizeof(occ_copy));
        int side_copy = side_to_move;
        int ep_copy = ep_square;
        int castle_copy = castle_rights;

        if (!make_move(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply)) {
            memcpy(pieces, pieces_copy, sizeof(pieces_copy));
            memcpy(occupancy, occ_copy, sizeof(occ_copy));
            side_to_move = side_copy;
            ep_square = ep_copy;
            castle_rights = castle_copy;
            continue;
        }
        legal_moves_count++;

        int score = -quiescence(-beta, -alpha, pieces, occupancy, side_to_move, ep_square, castle_rights, ply + 1);

        memcpy(pieces, pieces_copy, sizeof(pieces_copy));
        memcpy(occupancy, occ_copy, sizeof(occ_copy));
        side_to_move = side_copy;
        ep_square = ep_copy;
        castle_rights = castle_copy;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    if (checked && legal_moves_count == 0) return -MATE_SCORE + ply;
    return alpha;
}

static int negamax(int alpha, int beta, int depth, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int ply, int follow_pv) {
    pv_length[ply] = ply;
    if (depth <= 0 || ply >= MAX_PLY - 1)
        return quiescence(alpha, beta, pieces, occupancy, side_to_move, ep_square, castle_rights, ply);

    nodes_searched++;

    MoveList pseudo_moves;
    generate_all_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    int move_scores[sizeof(pseudo_moves.moves) / sizeof(pseudo_moves.moves[0])];
    for (int i = 0; i < pseudo_moves.count; i++) {
        Move pv_move = follow_pv && ply < previous_pv_length ? previous_pv[ply] : 0;
        move_scores[i] = score_move(pseudo_moves.moves[i], pieces, pv_move, ply, side_to_move);
    }

    int legal_moves_count = 0;

    for (int i = 0; i < pseudo_moves.count; i++) {

        pick_next_move(&pseudo_moves, i, move_scores);
        Move m = pseudo_moves.moves[i];

        Bitboard pieces_copy[12];
        Bitboard occ_copy[3];
        memcpy(pieces_copy, pieces, sizeof(pieces_copy));
        memcpy(occ_copy, occupancy, sizeof(occ_copy));
        int side_copy = side_to_move;
        int ep_copy = ep_square;
        int castle_copy = castle_rights;

        if (!make_move(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, ply)) {
            memcpy(pieces, pieces_copy, sizeof(pieces_copy));
            memcpy(occupancy, occ_copy, sizeof(occ_copy));
            side_to_move = side_copy;
            ep_square = ep_copy;
            castle_rights = castle_copy;
            continue;
        }

        legal_moves_count++;

        int score = -negamax(-beta, -alpha, depth - 1, pieces, occupancy, side_to_move, ep_square, castle_rights, ply + 1,
                             follow_pv && ply < previous_pv_length && m == previous_pv[ply]);

        memcpy(pieces, pieces_copy, sizeof(pieces_copy));
        memcpy(occupancy, occ_copy, sizeof(occ_copy));
        side_to_move = side_copy;
        ep_square = ep_copy;
        castle_rights = castle_copy;

        if (score >= beta) {

            if (!MOVE_IS_CAPTURE(m) && !MOVE_PROMOTED(m)) {
                if (killer_moves[0][ply] != m) {
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = m;
                }

                int src_sq = MOVE_SRC(m);
                int target_sq = MOVE_TARGET(m);
                int *history = &history_moves[side_to_move][src_sq][target_sq];
                int bonus = depth * depth;
                if (bonus > HISTORY_LIMIT) bonus = HISTORY_LIMIT;
                *history += bonus - (*history * bonus) / HISTORY_LIMIT;
            }

            return beta;
        }
        if (score > alpha) {
            alpha = score;
            pv_table[ply][ply] = m;
            for (int j = ply + 1; j < pv_length[ply + 1]; j++) {
                pv_table[ply][j] = pv_table[ply + 1][j];
            }
            pv_length[ply] = pv_length[ply + 1];
        }

    }

    if (legal_moves_count == 0) {
        int own_king = (side_to_move == WHITE) ? K : k;
        int king_sq = get_lsb_index(pieces[own_king]);
        int enemy_side = (side_to_move == WHITE) ? BLACK : WHITE;

        if (is_square_attacked(king_sq, pieces, occupancy, enemy_side)) {
            return -MATE_SCORE + ply;
        } else {
            return 0;
        }
    }

    return alpha;
}

Move search_best_move(Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int max_depth) {
    nodes_searched = 0;
    clear_search_heuristics();

    Move best_move = 0;
    int best_score = -INFINITY_SCORE;

    for (int current_depth = 1; current_depth <= max_depth && current_depth < MAX_PLY; current_depth++) {
        int alpha = -INFINITY_SCORE;
        int beta = INFINITY_SCORE;
        Move current_best_move = 0;
        int found_legal_move = 0;

        MoveList pseudo_moves;
        generate_all_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

        int move_scores[sizeof(pseudo_moves.moves) / sizeof(pseudo_moves.moves[0])];
        for (int i = 0; i < pseudo_moves.count; i++) {
            move_scores[i] = score_move(pseudo_moves.moves[i], pieces, best_move, 0, side_to_move);
        }

        for (int i = 0; i < pseudo_moves.count; i++) {

            pick_next_move(&pseudo_moves, i, move_scores);
            Move m = pseudo_moves.moves[i];

            Bitboard pieces_copy[12];
            Bitboard occ_copy[3];
            memcpy(pieces_copy, pieces, sizeof(pieces_copy));
            memcpy(occ_copy, occupancy, sizeof(occ_copy));
            int side_copy = side_to_move;
            int ep_copy = ep_square;
            int castle_copy = castle_rights;

            if (!make_move(m, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, 0)) {
                memcpy(pieces, pieces_copy, sizeof(pieces_copy));
                memcpy(occupancy, occ_copy, sizeof(occ_copy));
                side_to_move = side_copy;
                ep_square = ep_copy;
                castle_rights = castle_copy;
                continue;
            }

            found_legal_move = 1;
            int score = -negamax(-beta, -alpha, current_depth - 1, pieces, occupancy, side_to_move, ep_square, castle_rights, 1,
                                 previous_pv_length > 0 && m == previous_pv[0]);

            memcpy(pieces, pieces_copy, sizeof(pieces_copy));
            memcpy(occupancy, occ_copy, sizeof(occ_copy));
            side_to_move = side_copy;
            ep_square = ep_copy;
            castle_rights = castle_copy;

            if (score > alpha) {
                alpha = score;
                current_best_move = m;
                pv_table[0][0] = m;
                for (int j = 1; j < pv_length[1]; j++)
                    pv_table[0][j] = pv_table[1][j];
                pv_length[0] = pv_length[1];
            }
        }

        if (!found_legal_move) {
            break;
        }

        if (current_best_move != 0) {
            best_move = current_best_move;
            best_score = alpha;
            previous_pv_length = pv_length[0];
            for (int j = 0; j < previous_pv_length; j++) previous_pv[j] = pv_table[0][j];
        }

        printf("Search depth: %d | Nodes searched: %" PRIu64 " | Best score: %d\n", current_depth, nodes_searched, alpha);

        if (best_score >= MATE_SCORE - 100 || best_score <= -MATE_SCORE + 100) {
            break;
        }
    }

    return best_move;
}