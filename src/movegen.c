#include "../nnue/cce_nnue.h"
#include "../inc/movegen.h"
#include "../inc/magic.h"

// Global undo stack (indexed by ply)
UndoState undo_stack[MAX_PLY];

void add_move(MoveList *move_list, Move move) {
    // protect the fixed move list even for malformed FEN positions.
    if (move_list->count >= (int)(sizeof(move_list->moves) / sizeof(move_list->moves[0]))) return;
    move_list->moves[move_list->count] = move;
    move_list->count++;
}


Bitboard get_white_single_pushes(Bitboard wpawns, Bitboard occupied) {
    return (wpawns << 8) & ~occupied;
}

Bitboard get_white_double_pushes(Bitboard single_pushes, Bitboard occupied) {
    return ((single_pushes & RANK_3_MASK) << 8) & ~occupied;
}

Bitboard get_black_single_pushes(Bitboard bpawns, Bitboard occupied) {
    return (bpawns >> 8) & ~occupied;
}

Bitboard get_black_double_pushes(Bitboard single_pushes, Bitboard occupied) {
    return ((single_pushes & RANK_6_MASK) >> 8) & ~occupied;
}

Bitboard get_white_pawn_attack_west(Bitboard wpawns) { return (wpawns & NOT_A_FILE) << 7; }
Bitboard get_white_pawn_attack_east(Bitboard wpawns) { return (wpawns & NOT_H_FILE) << 9; }
Bitboard get_black_pawn_attack_west(Bitboard bpawns) { return (bpawns & NOT_A_FILE) >> 9; }
Bitboard get_black_pawn_attack_east(Bitboard bpawns) { return (bpawns & NOT_H_FILE) >> 7; }


static Bitboard mask_knight_attacks(int sq) {
    Bitboard bb = (1ULL << sq);
    Bitboard a = 0ULL;
    a |= (bb << 15) & NOT_H_FILE;
    a |= (bb << 17) & NOT_A_FILE;
    a |= (bb << 6)  & NOT_GH_FILE;
    a |= (bb << 10) & NOT_AB_FILE;
    a |= (bb >> 15) & NOT_A_FILE;
    a |= (bb >> 17) & NOT_H_FILE;
    a |= (bb >> 6)  & NOT_AB_FILE;
    a |= (bb >> 10) & NOT_GH_FILE;
    return a;
}

static Bitboard mask_king_attacks(int sq) {
    Bitboard bb = (1ULL << sq);
    Bitboard a = 0ULL;
    a |= (bb << 1) & NOT_A_FILE;
    a |= (bb << 7) & NOT_H_FILE;
    a |= (bb << 9) & NOT_A_FILE;
    a |= (bb >> 1) & NOT_H_FILE;
    a |= (bb >> 7) & NOT_A_FILE;
    a |= (bb >> 9) & NOT_H_FILE;
    a |=  bb << 8;
    a |=  bb >> 8;
    return a;
}

static Bitboard mask_pawn_attacks(int side, int sq) {
    Bitboard bb = (1ULL << sq);
    Bitboard a = 0ULL;
    if (side == WHITE) {
        a |= (bb << 7) & NOT_H_FILE;
        a |= (bb << 9) & NOT_A_FILE;
    } else {
        a |= (bb >> 7) & NOT_A_FILE;
        a |= (bb >> 9) & NOT_H_FILE;
    }
    return a;
}

Bitboard pawn_attacks[2][64];
Bitboard knight_attacks[64];
Bitboard king_attacks[64];

void init_leaper_attacks(void) {
    for (int sq = 0; sq < 64; sq++) {
        pawn_attacks[WHITE][sq] = mask_pawn_attacks(WHITE, sq);
        pawn_attacks[BLACK][sq] = mask_pawn_attacks(BLACK, sq);
        knight_attacks[sq]      = mask_knight_attacks(sq);
        king_attacks[sq]        = mask_king_attacks(sq);
    }
}


void generate_pawn_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq) {
    if (side_to_move == WHITE) {
        Bitboard wpawns = pieces[P];

        while (wpawns) {
            int src = pop_lsb(&wpawns);

            Bitboard single_push = ((1ULL << src) << 8) & ~occupancy[BOTH];
            if (single_push) {
                int target = get_lsb_index(single_push);

                if (target >= A8 && target <= H8) {
                    add_move(move_list, ENCODE_MOVE(src, target, P, Q, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, R, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, B, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, N, 0, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, P, 0, 0, 0, 0, 0));

                    Bitboard double_push = ((1ULL << target) << 8) & ~occupancy[BOTH];
                    if ((1ULL << target) & RANK_3_MASK && double_push) {
                        int double_target = get_lsb_index(double_push);
                        add_move(move_list, ENCODE_MOVE(src, double_target, P, 0, 0, 1, 0, 0));
                    }
                }
            }

            // kings are never captured; checkmate ends the game.
            Bitboard attacks = pawn_attacks[WHITE][src] & (occupancy[BLACK] & ~pieces[k]);
            while (attacks) {
                int target = pop_lsb(&attacks);
                if (target >= A8 && target <= H8) {
                    add_move(move_list, ENCODE_MOVE(src, target, P, Q, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, R, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, B, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, N, 1, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, P, 0, 1, 0, 0, 0));
                }
            }
        }

        if (ep_sq != NO_SQUARE) {
            Bitboard attackers = pawn_attacks[BLACK][ep_sq] & pieces[P];
            while (attackers) {
                int src = pop_lsb(&attackers);
                add_move(move_list, ENCODE_MOVE(src, ep_sq, P, 0, 1, 0, 1, 0));
            }
        }
    } else {
        Bitboard bpawns = pieces[p];

        while (bpawns) {
            int src = pop_lsb(&bpawns);

            Bitboard single_push = ((1ULL << src) >> 8) & ~occupancy[BOTH];
            if (single_push) {
                int target = get_lsb_index(single_push);

                if (target >= A1 && target <= H1) {
                    add_move(move_list, ENCODE_MOVE(src, target, p, q, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, r, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, b, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, n, 0, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, p, 0, 0, 0, 0, 0));

                    Bitboard double_push = ((1ULL << target) >> 8) & ~occupancy[BOTH];
                    if ((1ULL << target) & RANK_6_MASK && double_push) {
                        int double_target = get_lsb_index(double_push);
                        add_move(move_list, ENCODE_MOVE(src, double_target, p, 0, 0, 1, 0, 0));
                    }
                }
            }

            Bitboard attacks = pawn_attacks[BLACK][src] & (occupancy[WHITE] & ~pieces[K]);
            while (attacks) {
                int target = pop_lsb(&attacks);
                if (target >= A1 && target <= H1) {
                    add_move(move_list, ENCODE_MOVE(src, target, p, q, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, r, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, b, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, n, 1, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, p, 0, 1, 0, 0, 0));
                }
            }
        }

        if (ep_sq != NO_SQUARE) {
            Bitboard attackers = pawn_attacks[WHITE][ep_sq] & pieces[p];
            while (attackers) {
                int src = pop_lsb(&attackers);
                add_move(move_list, ENCODE_MOVE(src, ep_sq, p, 0, 1, 0, 1, 0));
            }
        }
    }
}

void generate_piece_moves(MoveList *move_list, int piece_type, Bitboard piece_bb, Bitboard own_occ, Bitboard enemy_occ, Bitboard both_occ) {
    while (piece_bb) {
        int src = pop_lsb(&piece_bb);
        Bitboard attacks = 0ULL;
        switch (piece_type) {
            case N: case n: attacks = knight_attacks[src]; break;
            case B: case b: attacks = get_bishop_attacks(src, both_occ); break;
            case R: case r: attacks = get_rook_attacks(src, both_occ); break;
            case Q: case q: attacks = get_queen_attacks(src, both_occ); break;
            case K: case k: attacks = king_attacks[src]; break;
        }

        // the excluded occupied square is the opposing king.
        attacks &= ~own_occ & ~(both_occ & ~own_occ & ~enemy_occ);

        while (attacks) {
            int target = pop_lsb(&attacks);
            int is_capture = (enemy_occ & (1ULL << target)) ? 1 : 0;
            add_move(move_list, ENCODE_MOVE(src, target, piece_type, 0, is_capture, 0, 0, 0));
        }
    }
}

void generate_all_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq, int castle_rights) {
    move_list->count = 0;

    int own_color   = side_to_move;
    int enemy_color = (side_to_move == WHITE) ? BLACK : WHITE;
    // keep the enemy king as a blocker but exclude it as a capture target.
    Bitboard capturable_enemy = occupancy[enemy_color] & ~pieces[side_to_move == WHITE ? k : K];

    generate_pawn_moves(move_list, pieces, occupancy, side_to_move, ep_sq);
    int offset = (side_to_move == WHITE) ? 0 : 6;
    generate_piece_moves(move_list, N + offset, pieces[N + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, B + offset, pieces[B + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, R + offset, pieces[R + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, Q + offset, pieces[Q + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, K + offset, pieces[K + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);

    generate_castling_moves(move_list, pieces, occupancy, side_to_move, castle_rights);
}

// Avoid generating quiet piece moves and castling at q-nodes.
void generate_tactical_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq) {
    move_list->count = 0;
    generate_pawn_moves(move_list, pieces, occupancy, side_to_move, ep_sq);
    int count = 0;
    for (int i = 0; i < move_list->count; i++) {
        Move m = move_list->moves[i];
        if (MOVE_IS_CAPTURE(m) || MOVE_PROMOTED(m)) move_list->moves[count++] = m;
    }
    move_list->count = count;
    int offset = side_to_move == WHITE ? 0 : 6;
    Bitboard enemy = occupancy[side_to_move ^ 1] & ~pieces[side_to_move == WHITE ? k : K];
    for (int type = N; type <= K; type++) {
        if (type == P) continue;
        Bitboard active = pieces[type + offset];
        while (active) {
            int src = pop_lsb(&active);
            Bitboard attacks = 0;
            switch (type) {
                case N: attacks = knight_attacks[src]; break;
                case B: attacks = get_bishop_attacks(src, occupancy[BOTH]); break;
                case R: attacks = get_rook_attacks(src, occupancy[BOTH]); break;
                case Q: attacks = get_queen_attacks(src, occupancy[BOTH]); break;
                case K: attacks = king_attacks[src]; break;
            }
            attacks &= enemy;
            while (attacks) {
                int dst = pop_lsb(&attacks);
                add_move(move_list, ENCODE_MOVE(src, dst, type + offset, 0, 1, 0, 0, 0));
            }
        }
    }
}

void generate_castling_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int castle_rights) {
    if (side_to_move == WHITE) {
        if ((castle_rights & WK_RIGHT) && TEST_BIT(pieces[K], E1) && TEST_BIT(pieces[R], H1) && !(occupancy[BOTH] & WK_PATH)) {
            if (!is_square_attacked(E1, pieces, occupancy, BLACK) &&
                !is_square_attacked(F1, pieces, occupancy, BLACK) &&
                !is_square_attacked(G1, pieces, occupancy, BLACK)) {
                add_move(move_list, ENCODE_MOVE(E1, G1, K, 0, 0, 0, 0, 1));
            }
        }

        if ((castle_rights & WQ_RIGHT) && TEST_BIT(pieces[K], E1) && TEST_BIT(pieces[R], A1) && !(occupancy[BOTH] & WQ_PATH)) {
            if (!is_square_attacked(E1, pieces, occupancy, BLACK) &&
                !is_square_attacked(D1, pieces, occupancy, BLACK) &&
                !is_square_attacked(C1, pieces, occupancy, BLACK)) {
                add_move(move_list, ENCODE_MOVE(E1, C1, K, 0, 0, 0, 0, 1));
            }
        }
    } else {
        if ((castle_rights & BK_RIGHT) && TEST_BIT(pieces[k], E8) && TEST_BIT(pieces[r], H8) && !(occupancy[BOTH] & BK_PATH)) {
            if (!is_square_attacked(E8, pieces, occupancy, WHITE) &&
                !is_square_attacked(F8, pieces, occupancy, WHITE) &&
                !is_square_attacked(G8, pieces, occupancy, WHITE)) {
                add_move(move_list, ENCODE_MOVE(E8, G8, k, 0, 0, 0, 0, 1));
            }
        }

        if ((castle_rights & BQ_RIGHT) && TEST_BIT(pieces[k], E8) && TEST_BIT(pieces[r], A8) && !(occupancy[BOTH] & BQ_PATH)) {
            if (!is_square_attacked(E8, pieces, occupancy, WHITE) &&
                !is_square_attacked(D8, pieces, occupancy, WHITE) &&
                !is_square_attacked(C8, pieces, occupancy, WHITE)) {
                add_move(move_list, ENCODE_MOVE(E8, C8, k, 0, 0, 0, 0, 1));
            }
        }
    }
}


static const int castling_rights_update[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11
};

int make_move(Move move, Bitboard pieces[12], Bitboard occupancy[3],
              int *side_to_move, int *ep_square, int *castle_rights, int ply) {

    int src        = MOVE_SRC(move);
    int target     = MOVE_TARGET(move);
    int piece      = MOVE_PIECE(move);
    int promoted   = MOVE_PROMOTED(move);
    int capture    = MOVE_IS_CAPTURE(move);
    int double_push= MOVE_IS_DOUBLE(move);
    int en_passant = MOVE_IS_EP(move);
    int castle     = MOVE_IS_CASTLING(move);

    int own_side   = *side_to_move;
    int enemy_side = own_side ^ 1;   // WHITE^1=BLACK, BLACK^1=WHITE

    // reject malformed or king-capturing moves before touching the undo stack.
    if (piece > k || !(pieces[piece] & (1ULL << src)) ||
        (pieces[enemy_side == WHITE ? K : k] & (1ULL << target))) return 0;

    // Prepare NNUE before board mutation; rejected moves do not advance it.
    cce_nnue_prepare(move, pieces, *side_to_move, *ep_square, *castle_rights, ply);

    undo_stack[ply].move          = move;
    undo_stack[ply].ep_square     = *ep_square;
    undo_stack[ply].castle_rights = *castle_rights;
    undo_stack[ply].captured_piece = NO_PIECE;

    if (capture && !en_passant) {
        int start_p = (enemy_side == WHITE) ? P : p;
        int end_p   = (enemy_side == WHITE) ? K : k;
        for (int pt = start_p; pt <= end_p; pt++) {
            if (TEST_BIT(pieces[pt], target)) {
                undo_stack[ply].captured_piece = pt;
                CLEAR_BIT(pieces[pt], target);
                break;
            }
        }
    }

    CLEAR_BIT(pieces[piece], src);
    SET_BIT(pieces[piece], target);

    if (en_passant) {
        int ep_pawn_sq = (own_side == WHITE) ? (target - 8) : (target + 8);
        int enemy_pawn = (own_side == WHITE) ? p : P;
        undo_stack[ply].captured_piece = enemy_pawn;
        CLEAR_BIT(pieces[enemy_pawn], ep_pawn_sq);
    }

    if (promoted) {
        CLEAR_BIT(pieces[piece], target);
        SET_BIT(pieces[promoted], target);
    }

    if (castle) {
        switch (target) {
            case G1: CLEAR_BIT(pieces[R], H1); SET_BIT(pieces[R], F1); break;
            case C1: CLEAR_BIT(pieces[R], A1); SET_BIT(pieces[R], D1); break;
            case G8: CLEAR_BIT(pieces[r], H8); SET_BIT(pieces[r], F8); break;
            case C8: CLEAR_BIT(pieces[r], A8); SET_BIT(pieces[r], D8); break;
        }
    }

    *castle_rights &= castling_rights_update[src];
    *castle_rights &= castling_rights_update[target];

    *ep_square = NO_SQUARE;
    if (double_push) {
        *ep_square = (own_side == WHITE) ? (target - 8) : (target + 8);
    }

    // promotions change piece type, not occupied squares.
    // Update only the source, destination, capture, and castling rook squares.
    occupancy[own_side] &= ~(1ULL << src);
    occupancy[own_side] |= 1ULL << target;
    if (capture && !en_passant && undo_stack[ply].captured_piece != NO_PIECE)
        occupancy[enemy_side] &= ~(1ULL << target);
    if (en_passant)
        occupancy[enemy_side] &= ~(1ULL << (own_side == WHITE ? target - 8 : target + 8));
    if (castle) {
        int rook_src = target == G1 ? H1 : target == C1 ? A1 : target == G8 ? H8 : A8;
        int rook_dst = target == G1 ? F1 : target == C1 ? D1 : target == G8 ? F8 : D8;
        occupancy[own_side] &= ~(1ULL << rook_src);
        occupancy[own_side] |= 1ULL << rook_dst;
    }
    occupancy[BOTH]  = occupancy[WHITE] | occupancy[BLACK];

    int own_king = (own_side == WHITE) ? K : k;
    int king_sq  = get_lsb_index(pieces[own_king]);

    if (is_square_attacked(king_sq, pieces, occupancy, enemy_side)) {
        unmake_move(move, pieces, occupancy, side_to_move, ep_square, castle_rights, ply);
        return 0;
    }

    cce_nnue_commit(move, ply);
    *side_to_move = enemy_side;
    return 1;
}

void unmake_move(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                 int *side_to_move, int *ep_square, int *castle_rights, int ply) {
    cce_nnue_unmake(move, ply);

    // The side that made this move is now the *enemy* (since make_move
    // switches side_to_move before we get here only for legal moves;
    // for illegal moves make_move calls us before switching, so we use
    // the stored state to be safe).
    // We derive own_side from the move piece color directly.
    int src        = MOVE_SRC(move);
    int target     = MOVE_TARGET(move);
    int piece      = MOVE_PIECE(move);
    int promoted   = MOVE_PROMOTED(move);
    int en_passant = MOVE_IS_EP(move);
    int castle     = MOVE_IS_CASTLING(move);

    int own_side = (piece <= K) ? WHITE : BLACK;   // mover's color

    if (promoted) {
        CLEAR_BIT(pieces[promoted], target);
        SET_BIT(pieces[piece], target);    // put pawn back at target
    }

    CLEAR_BIT(pieces[piece], target);
    SET_BIT(pieces[piece], src);

    int cap = undo_stack[ply].captured_piece;
    if (cap != NO_PIECE) {
        if (en_passant) {
            // EP: captured pawn is NOT on target; restore it to the right sq
            int ep_pawn_sq = (own_side == WHITE) ? (target - 8) : (target + 8);
            SET_BIT(pieces[cap], ep_pawn_sq);
        } else {
            SET_BIT(pieces[cap], target);
        }
    }

    if (castle) {
        switch (target) {
            case G1: CLEAR_BIT(pieces[R], F1); SET_BIT(pieces[R], H1); break;
            case C1: CLEAR_BIT(pieces[R], D1); SET_BIT(pieces[R], A1); break;
            case G8: CLEAR_BIT(pieces[r], F8); SET_BIT(pieces[r], H8); break;
            case C8: CLEAR_BIT(pieces[r], D8); SET_BIT(pieces[r], A8); break;
        }
    }

    *ep_square     = undo_stack[ply].ep_square;
    *castle_rights = undo_stack[ply].castle_rights;
    *side_to_move  = own_side;   // restore mover's side

    // reverse the square changes recorded by the move.
    occupancy[own_side] &= ~(1ULL << target);
    occupancy[own_side] |= 1ULL << src;
    if (cap != NO_PIECE)
        occupancy[own_side ^ 1] |= 1ULL << (en_passant
                          ? (own_side == WHITE ? target - 8 : target + 8) : target);
    if (castle) {
        int rook_src = target == G1 ? H1 : target == C1 ? A1 : target == G8 ? H8 : A8;
        int rook_dst = target == G1 ? F1 : target == C1 ? D1 : target == G8 ? F8 : D8;
        occupancy[own_side] &= ~(1ULL << rook_dst);
        occupancy[own_side] |= 1ULL << rook_src;
    }
    occupancy[BOTH]  = occupancy[WHITE] | occupancy[BLACK];
}
