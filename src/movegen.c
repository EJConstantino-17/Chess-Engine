#include "movegen.h"
#include "magic.h"

// Global undo stack (indexed by ply)
UndoState undo_stack[MAX_PLY];

void add_move(MoveList *move_list, Move move) {
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

// ---------------------------------------------------------------------------
// Pawn push helpers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Leaper attack masks
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Move generation
// ---------------------------------------------------------------------------

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

            Bitboard attacks = pawn_attacks[WHITE][src] & occupancy[BLACK];
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

            Bitboard attacks = pawn_attacks[BLACK][src] & occupancy[WHITE];
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

        attacks &= ~own_occ;

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

    generate_pawn_moves(move_list, pieces, occupancy, side_to_move, ep_sq);
    int offset = (side_to_move == WHITE) ? 0 : 6;
    generate_piece_moves(move_list, N + offset, pieces[N + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    generate_piece_moves(move_list, B + offset, pieces[B + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    generate_piece_moves(move_list, R + offset, pieces[R + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    generate_piece_moves(move_list, Q + offset, pieces[Q + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    generate_piece_moves(move_list, K + offset, pieces[K + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);

    generate_castling_moves(move_list, pieces, occupancy, side_to_move, castle_rights);
}

void generate_castling_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int castle_rights) {
    if (side_to_move == WHITE) {
        if ((castle_rights & WK_RIGHT) && !(occupancy[BOTH] & WK_PATH)) {
            if (!is_square_attacked(E1, pieces, occupancy, BLACK) &&
                !is_square_attacked(F1, pieces, occupancy, BLACK) &&
                !is_square_attacked(G1, pieces, occupancy, BLACK)) {
                add_move(move_list, ENCODE_MOVE(E1, G1, K, 0, 0, 0, 0, 1));
            }
        }

        if ((castle_rights & WQ_RIGHT) && !(occupancy[BOTH] & WQ_PATH)) {
            if (!is_square_attacked(E1, pieces, occupancy, BLACK) &&
                !is_square_attacked(D1, pieces, occupancy, BLACK) &&
                !is_square_attacked(C1, pieces, occupancy, BLACK)) {
                add_move(move_list, ENCODE_MOVE(E1, C1, K, 0, 0, 0, 0, 1));
            }
        }
    } else {
        if ((castle_rights & BK_RIGHT) && !(occupancy[BOTH] & BK_PATH)) {
            if (!is_square_attacked(E8, pieces, occupancy, WHITE) &&
                !is_square_attacked(F8, pieces, occupancy, WHITE) &&
                !is_square_attacked(G8, pieces, occupancy, WHITE)) {
                add_move(move_list, ENCODE_MOVE(E8, G8, k, 0, 0, 0, 0, 1));
            }
        }

        if ((castle_rights & BQ_RIGHT) && !(occupancy[BOTH] & BQ_PATH)) {
            if (!is_square_attacked(E8, pieces, occupancy, WHITE) &&
                !is_square_attacked(D8, pieces, occupancy, WHITE) &&
                !is_square_attacked(C8, pieces, occupancy, WHITE)) {
                add_move(move_list, ENCODE_MOVE(E8, C8, k, 0, 0, 0, 0, 1));
            }
        }
    }
}

int is_square_attacked(int sq, Bitboard pieces[12], Bitboard occupancy[3], int attacker_side) {
    if (attacker_side == WHITE) {
        if (pawn_attacks[BLACK][sq] & pieces[P]) return 1;
    } else {
        if (pawn_attacks[WHITE][sq] & pieces[p]) return 1;
    }

    int knight_piece = (attacker_side == WHITE) ? N : n;
    if (knight_attacks[sq] & pieces[knight_piece]) return 1;

    int king_piece = (attacker_side == WHITE) ? K : k;
    if (king_attacks[sq] & pieces[king_piece]) return 1;

    int bishop_piece = (attacker_side == WHITE) ? B : b;
    int queen_piece  = (attacker_side == WHITE) ? Q : q;
    Bitboard bishop_rays = get_bishop_attacks(sq, occupancy[BOTH]);
    if (bishop_rays & (pieces[bishop_piece] | pieces[queen_piece])) return 1;

    int rook_piece = (attacker_side == WHITE) ? R : r;
    Bitboard rook_rays = get_rook_attacks(sq, occupancy[BOTH]);
    if (rook_rays & (pieces[rook_piece] | pieces[queen_piece])) return 1;

    return 0;
}

// ---------------------------------------------------------------------------
// Castling rights update table
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// make_move  — saves undo info into undo_stack[ply], applies move, returns
//              1 if legal, 0 if the king is left in check (board is restored).
// ---------------------------------------------------------------------------

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

    // ------------------------------------------------------------------
    // 1. Save irreversible state BEFORE modifying anything
    // ------------------------------------------------------------------
    undo_stack[ply].move          = move;
    undo_stack[ply].ep_square     = *ep_square;
    undo_stack[ply].castle_rights = *castle_rights;
    undo_stack[ply].captured_piece = NO_PIECE;

    // ------------------------------------------------------------------
    // 2. Identify and clear captured piece BEFORE landing the moving piece
    //    (prevents bitboard corruption when e.g. pieces[R] and pieces[r]
    //     would momentarily share the same bit)
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // 3. Move the piece (src -> target)
    // ------------------------------------------------------------------
    CLEAR_BIT(pieces[piece], src);
    SET_BIT(pieces[piece], target);

    // ------------------------------------------------------------------
    // 4. En passant capture — remove the captured pawn
    // ------------------------------------------------------------------
    if (en_passant) {
        int ep_pawn_sq = (own_side == WHITE) ? (target - 8) : (target + 8);
        int enemy_pawn = (own_side == WHITE) ? p : P;
        undo_stack[ply].captured_piece = enemy_pawn;
        CLEAR_BIT(pieces[enemy_pawn], ep_pawn_sq);
    }

    // ------------------------------------------------------------------
    // 5. Promotion — replace pawn with promoted piece
    // ------------------------------------------------------------------
    if (promoted) {
        CLEAR_BIT(pieces[piece], target);
        SET_BIT(pieces[promoted], target);
    }

    // ------------------------------------------------------------------
    // 6. Castling — move the rook
    // ------------------------------------------------------------------
    if (castle) {
        switch (target) {
            case G1: CLEAR_BIT(pieces[R], H1); SET_BIT(pieces[R], F1); break;
            case C1: CLEAR_BIT(pieces[R], A1); SET_BIT(pieces[R], D1); break;
            case G8: CLEAR_BIT(pieces[r], H8); SET_BIT(pieces[r], F8); break;
            case C8: CLEAR_BIT(pieces[r], A8); SET_BIT(pieces[r], D8); break;
        }
    }

    // ------------------------------------------------------------------
    // 7. Update castling rights and en-passant square
    // ------------------------------------------------------------------
    *castle_rights &= castling_rights_update[src];
    *castle_rights &= castling_rights_update[target];

    *ep_square = NO_SQUARE;
    if (double_push) {
        *ep_square = (own_side == WHITE) ? (target - 8) : (target + 8);
    }

    // ------------------------------------------------------------------
    // 8. Recalculate occupancy
    // ------------------------------------------------------------------
    occupancy[WHITE] = pieces[P] | pieces[N] | pieces[B] | pieces[R] | pieces[Q] | pieces[K];
    occupancy[BLACK] = pieces[p] | pieces[n] | pieces[b] | pieces[r] | pieces[q] | pieces[k];
    occupancy[BOTH]  = occupancy[WHITE] | occupancy[BLACK];

    // ------------------------------------------------------------------
    // 9. Legality check — if own king is in check, unmake and return 0
    // ------------------------------------------------------------------
    int own_king = (own_side == WHITE) ? K : k;
    int king_sq  = get_lsb_index(pieces[own_king]);

    if (is_square_attacked(king_sq, pieces, occupancy, enemy_side)) {
        unmake_move(move, pieces, occupancy, side_to_move, ep_square, castle_rights, ply);
        return 0;
    }

    // ------------------------------------------------------------------
    // 10. Legal move — switch side
    // ------------------------------------------------------------------
    *side_to_move = enemy_side;
    return 1;
}

// ---------------------------------------------------------------------------
// unmake_move — restores the board from undo_stack[ply]
// ---------------------------------------------------------------------------

void unmake_move(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                 int *side_to_move, int *ep_square, int *castle_rights, int ply) {

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

    // ------------------------------------------------------------------
    // 1. Undo promotion — restore pawn on target
    // ------------------------------------------------------------------
    if (promoted) {
        CLEAR_BIT(pieces[promoted], target);
        SET_BIT(pieces[piece], target);    // put pawn back at target
    }

    // ------------------------------------------------------------------
    // 2. Move piece back (target -> src)
    // ------------------------------------------------------------------
    CLEAR_BIT(pieces[piece], target);
    SET_BIT(pieces[piece], src);

    // ------------------------------------------------------------------
    // 3. Restore captured piece
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // 4. Undo castling rook move
    // ------------------------------------------------------------------
    if (castle) {
        switch (target) {
            case G1: CLEAR_BIT(pieces[R], F1); SET_BIT(pieces[R], H1); break;
            case C1: CLEAR_BIT(pieces[R], D1); SET_BIT(pieces[R], A1); break;
            case G8: CLEAR_BIT(pieces[r], F8); SET_BIT(pieces[r], H8); break;
            case C8: CLEAR_BIT(pieces[r], D8); SET_BIT(pieces[r], A8); break;
        }
    }

    // ------------------------------------------------------------------
    // 5. Restore irreversible state from undo stack
    // ------------------------------------------------------------------
    *ep_square     = undo_stack[ply].ep_square;
    *castle_rights = undo_stack[ply].castle_rights;
    *side_to_move  = own_side;   // restore mover's side

    // ------------------------------------------------------------------
    // 6. Recalculate occupancy
    // ------------------------------------------------------------------
    occupancy[WHITE] = pieces[P] | pieces[N] | pieces[B] | pieces[R] | pieces[Q] | pieces[K];
    occupancy[BLACK] = pieces[p] | pieces[n] | pieces[b] | pieces[r] | pieces[q] | pieces[k];
    occupancy[BOTH]  = occupancy[WHITE] | occupancy[BLACK];
}