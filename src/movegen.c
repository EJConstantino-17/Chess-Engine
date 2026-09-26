#include "movegen.h"
#include "moves.h"
#include "magic.h"

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

Bitboard get_white_pawn_attack_west(Bitboard wpawns) {
    return (wpawns & NOT_A_FILE) << 7; 
}

Bitboard get_white_pawn_attack_east(Bitboard wpawns) {
    return (wpawns & NOT_H_FILE) << 9; 
}

Bitboard get_black_pawn_attack_west(Bitboard bpawns) {
    return (bpawns & NOT_A_FILE) >> 9; 
}

Bitboard get_black_pawn_attack_east(Bitboard bpawns) {
    return (bpawns & NOT_H_FILE) >> 7; 
}

Bitboard mask_knight_attacks(int sq) {
    Bitboard bitboard = (1ULL << sq);
    Bitboard attacks = 0ULL;

    attacks |= (bitboard << 15) & NOT_H_FILE;
    attacks |= (bitboard << 17) & NOT_A_FILE;
    attacks |= (bitboard << 6) & NOT_GH_FILE;
    attacks |= (bitboard << 10) & NOT_AB_FILE;

    attacks |= (bitboard >> 15) & NOT_A_FILE;
    attacks |= (bitboard >> 17) & NOT_H_FILE;
    attacks |= (bitboard >> 6) & NOT_AB_FILE;
    attacks |= (bitboard >> 10) & NOT_GH_FILE;

    return attacks;
}

Bitboard knight_attacks[64];

void init_knight_attacks(void) {
    for (int sq = 0; sq < 64; sq++) {
        knight_attacks[sq] = mask_knight_attacks(sq);
    }
}

Bitboard mask_king_attacks(int sq) {
    Bitboard bitboard = (1ULL << sq);
    Bitboard attacks = 0ULL;

    attacks |= (bitboard << 1) & NOT_A_FILE;
    attacks |= (bitboard << 7) & NOT_H_FILE;
    attacks |= (bitboard << 9) & NOT_A_FILE;

    attacks |= (bitboard >> 1) & NOT_H_FILE;
    attacks |= (bitboard >> 7) & NOT_A_FILE;
    attacks |= (bitboard >> 9) & NOT_H_FILE;

    attacks |= bitboard << 8;
    attacks |= bitboard >> 8;

    return attacks;
}

Bitboard king_attacks[64];

void init_king_attacks(void) {
    for (int sq = 0; sq < 64; sq++) {
        king_attacks[sq] = mask_king_attacks(sq);
    }
}

Bitboard mask_pawn_attacks(int side, int sq) {
    Bitboard bitboard = (1ULL << sq);
    Bitboard attacks = 0ULL;

    if (side == WHITE) {
        attacks |= (bitboard << 7 ) & NOT_H_FILE;
        attacks |= (bitboard << 9) & NOT_A_FILE;
    } else {
        attacks |= (bitboard >> 7) & NOT_A_FILE;
        attacks |= (bitboard >> 9) & NOT_H_FILE;
    }

    return attacks;
}

Bitboard pawn_attacks[2][64];

void init_pawn_attacks(void) {
    for (int sq = 0; sq < 64; sq++) {
        pawn_attacks[WHITE][sq] = mask_pawn_attacks(WHITE, sq);
        pawn_attacks[BLACK][sq] = mask_pawn_attacks(BLACK, sq);
    }
}

Bitboard get_bishop_attacks(int sq, Bitboard occupancy) {
    occupancy &= bishop_magics[sq].mask;
    occupancy *= bishop_magics[sq].magic;
    occupancy >>= bishop_magics[sq].shift;
    return bishop_magics[sq].attacks[occupancy];
}

Bitboard get_rook_attacks(int sq, Bitboard occupancy) {
    occupancy &= rook_magics[sq].mask;
    occupancy *= rook_magics[sq].magic;
    occupancy >>= rook_magics[sq].shift;
    return rook_magics[sq].attacks[occupancy];
}

Bitboard get_queen_attacks(int sq, Bitboard occupancy) {
    return get_bishop_attacks(sq, occupancy) | get_rook_attacks(sq, occupancy);
}

void generate_pawn_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq) {
    if (side_to_move == WHITE) {
        Bitboard wpawns = pieces[P];

        while(wpawns) {
            int src = pop_lsb(&wpawns);

            Bitboard single_push = ((1ULL << src) << 8 ) & ~occupancy[BOTH];
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
            while(attacks) {
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
            while(attackers) {
                int src = pop_lsb(&attackers);
                add_move(move_list, ENCODE_MOVE(src, ep_sq, P, 0, 1, 0, 1, 0));
            }
        }
    } else {
        Bitboard bpawns = pieces[p];

        while(bpawns) {
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
                        add_move(move_list, ENCODE_MOVE(src, double_target, p, 0, 0, 1, 0 , 0));
                    }
                }
            }

            Bitboard attacks = pawn_attacks[BLACK][src] & occupancy[WHITE];
            while(attacks) {
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
            while(attackers) {
                int src = pop_lsb(&attackers);
                add_move(move_list, ENCODE_MOVE(src, ep_sq, P, 0, 1, 0, 1, 0));
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

        while(attacks) {
            int target = pop_lsb(&attacks);
            int is_capture = (enemy_occ & (1ULL << target)) ? 1 : 0;

            add_move(move_list, ENCODE_MOVE(src, target, piece_type, 0, is_capture, 0, 0, 0));
        }
    }
}

void generate_all_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq, int castle_rights) {
    move_list->count = 0;

    int own_color = side_to_move;
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
            if ((!is_square_attacked(E1, pieces, occupancy, BLACK) &&
                !is_square_attacked(D1, pieces, occupancy, BLACK) &&
                !is_square_attacked(C1, pieces, occupancy, BLACK))) {
                add_move(move_list, ENCODE_MOVE(E1, C1, K, 0, 0, 0, 0, 1));
            }
        }
    } else {

        if ((castle_rights & BK_RIGHT) && !(occupancy[BOTH] & BK_PATH)) {
            if (!is_square_attacked(E8, pieces, occupancy, BLACK) &&
                !is_square_attacked(F8, pieces, occupancy, BLACK) &&
                !is_square_attacked(G8, pieces, occupancy, BLACK)) {
                add_move(move_list, ENCODE_MOVE(E8, G8, k, 0, 0, 0, 0, 1));
            }
        }

        if ((castle_rights & BQ_RIGHT) && !(occupancy[BOTH] & BQ_PATH)) {
            if ((!is_square_attacked(E8, pieces, occupancy, BLACK) &&
                !is_square_attacked(D8, pieces, occupancy, BLACK) &&
                !is_square_attacked(C8, pieces, occupancy, BLACK))) {
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
    int queen_piece = (attacker_side == WHITE) ? Q : q;
    Bitboard bishop_rays = get_bishop_attacks(sq, occupancy[BOTH]);
    if (bishop_rays & (pieces[bishop_piece] | pieces[queen_piece])) return 1;

    int rook_piece = (attacker_side == WHITE) ? R : r;
    Bitboard rook_rays = get_rook_attacks(sq, occupancy[BOTH]);
    if (rook_rays & (pieces[rook_piece] | pieces[queen_piece])) return 1;

    return 0;
}

const int castling_rights_update[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11
};

int make_move(Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side_to_move, int *ep_square, int *castle_rights) {
    int src =           MOVE_SRC(move);
    int target =        MOVE_TARGET(move);
    int piece =         MOVE_PIECE(move);
    int promoted =      MOVE_PROMOTED(move);
    int capture =       MOVE_IS_CAPTURE(move);
    int double_push =   MOVE_IS_DOUBLE(move);
    int en_passant =    MOVE_IS_EP(move);
    int castle =        MOVE_IS_CASTLING(move);

    int own_side = *side_to_move;
    int enemy_side = (own_side == WHITE) ? BLACK : WHITE;

    CLEAR_BIT(pieces[piece], src);
    SET_BIT(pieces[piece], target);

    if (capture && !en_passant) {
        int start_p = (enemy_side == WHITE) ? P : p;
        int end_p = (enemy_side == WHITE) ? K : k;

        for (int p_type = start_p; p_type <= end_p; p_type++) {
            if (TEST_BIT(pieces[p_type], target)) {
                CLEAR_BIT(pieces[p_type], target);
                break;
            }
        }
    }

    if (en_passant) {
        int ep_pawn_sq = (own_side == WHITE) ? (target - 8) : (target + 8);
        int enemy_pawn = (own_side == WHITE) ? p : P;
        CLEAR_BIT(pieces[enemy_pawn], ep_pawn_sq);
    }

    *ep_square = NO_SQUARE;

    if (double_push) {
        *ep_square = (own_side == WHITE) ? (target - 8) : (target + 8);
    }

    if (promoted) {
        CLEAR_BIT(pieces[piece], target);
        SET_BIT(pieces[promoted], target);
    }

    if (castle) {
        switch (target) {
            case G1: CLEAR_BIT(pieces[R], H1); SET_BIT(pieces[R], F1); break;
            case C1: CLEAR_BIT(pieces[R], A1); SET_BIT(pieces[R], D1); break;
            case G8: CLEAR_BIT(pieces[R], H8); SET_BIT(pieces[r], F8); break;
            case C8: CLEAR_BIT(pieces[R], A8); SET_BIT(pieces[r], D8); break;
        }
    }

    *castle_rights &= castling_rights_update[src];
    *castle_rights &= castling_rights_update[target];

    occupancy[WHITE] = pieces[P] | pieces[N] | pieces[B] | pieces[R] | pieces[Q] | pieces[K];
    occupancy[BLACK] = pieces[p] | pieces[n] | pieces[b] | pieces[r] | pieces[q] | pieces[k];
    occupancy[BOTH] = occupancy[WHITE] | occupancy[BLACK];

    int own_king = (own_side == WHITE) ? K : k;
    int king_sq = get_lsb_index(pieces[own_king]);

    if (is_square_attacked(king_sq, pieces, occupancy,  enemy_side)) return 0;

    *side_to_move = enemy_side;
    return 1;
}