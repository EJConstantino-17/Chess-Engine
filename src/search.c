#include "eval.h"
#include "movegen.h"
#include "../inc/search.h"
#include <string.h>
#include <stdio.h>

static Bitboard nodes_searched = 0;

static int quiescence(int alpha, int beta, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int ply) {
    nodes_searched++;

    int stand_pat = evaluate(pieces, occupancy, side_to_move);

    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList pseudo_moves;
    generate_all_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    for (int i = 0; i < pseudo_moves.count; i++) {
        Move m = pseudo_moves.moves[i];

        if (!MOVE_IS_CAPTURE(m) && !MOVE_PROMOTED(m)) continue;

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

        int score = -quiescence(-beta, -alpha, pieces, occupancy, side_to_move, ep_square, castle_rights, ply + 1);

        memcpy(pieces, pieces_copy, sizeof(pieces_copy));
        memcpy(occupancy, occ_copy, sizeof(occ_copy));
        side_to_move = side_copy;
        ep_square = ep_copy;
        castle_rights = castle_copy;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(int alpha, int beta, int depth, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int ply) {
    if (depth == 0) return quiescence(alpha, beta, pieces, occupancy, side_to_move, ep_square, castle_rights, ply);

    nodes_searched++;

    MoveList pseudo_moves;
    generate_all_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    int legal_moves_count = 0;

    for (int i = 0; i < pseudo_moves.count; i++) {
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

        int score = -negamax(-beta, -alpha, depth - 1, pieces, occupancy, side_to_move, ep_square, castle_rights, ply + 1);

        memcpy(pieces, pieces_copy, sizeof(pieces_copy));
        memcpy(occupancy, occ_copy, sizeof(occ_copy));
        side_to_move = side_copy;
        ep_square = ep_copy;
        castle_rights = castle_copy;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;

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
    Move best_move = 0;
    int best_score = -INFINITY_SCORE;

    for (int current_depth = 1; current_depth <= max_depth; current_depth++) {
        int alpha = -INFINITY_SCORE;
        int beta = INFINITY_SCORE;
        Move current_best_move = 0;
        int found_legal_move = 0;

        MoveList pseudo_moves;
        generate_all_moves(&pseudo_moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

        for (int i = 0; i < pseudo_moves.count; i++) {
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
            int score = -negamax(-beta, -alpha, current_depth - 1, pieces, occupancy, side_to_move, ep_square, castle_rights, 1);

            memcpy(pieces, pieces_copy, sizeof(pieces_copy));
            memcpy(occupancy, occ_copy, sizeof(occ_copy));
            side_to_move = side_copy;
            ep_square = ep_copy;
            castle_rights = castle_copy;

            if (score > alpha) {
                alpha = score;
                current_best_move = m;
            }
        }

        if (!found_legal_move) {
            break;
        }

        if (current_best_move != 0) {
            best_move = current_best_move;
            best_score = alpha;
        }

        printf("Search depth: %d | Nodes searched: %" PRIu64 " | Best score: %d\n", current_depth, nodes_searched, alpha);

        if (best_score >= MATE_SCORE - 100 || best_score <= -MATE_SCORE + 100) {
            break;
        }
    }

    return best_move;
}