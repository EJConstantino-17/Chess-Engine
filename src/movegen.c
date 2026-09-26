#include "../inc/movegen.h"
#include "../inc/moves.h"

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

MagicEntry bishop_magics[64];
MagicEntry rook_magics[64];

Bitboard get_bishop_attacks(int sq, Bitboard occupied) {
    Bitboard occ = occupied & bishop_magics[sq].mask;
    Bitboard idx = (Bitboard)((occ * bishop_magics[sq].magic) >> bishop_magics[sq].shift);
    return bishop_magics[sq].attacks[idx];
}

Bitboard get_rook_attacks(int sq, Bitboard occupied) {
    Bitboard occ = occupied & rook_magics[sq].mask;
    Bitboard idx = (Bitboard)((occ * rook_magics[sq].magic) >> rook_magics[sq].shift);
    return rook_magics[sq].attacks[idx];
}

Bitboard get_queen_attacks(int sq, Bitboard occupied) {
    return get_bishop_attacks(sq, occupied) | get_rook_attacks(sq, occupied);
}

void generate_pawn_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move) {
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
    }
}

void generate_piece_moves(MoveList *move_list, int piece_type, Bitboard piece_bb, Bitboard own_occ, Bitboard enemy_occ, Bitboard both_occ) {
    while (piece_bb) {
        int src = pop_lsb(&piece_bb);
        Bitboard attacks;
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

void generate_all_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move) {
    move_list->count = 0;

    int own_color = side_to_move;
    int enemy_color = (side_to_move == WHITE) ? BLACK : WHITE;

    generate_pawn_moves(move_list, pieces, occupancy, side_to_move);
    int offset = (side_to_move == WHITE) ? 0 : 6;
    generate_piece_moves(move_list, N + offset, pieces[N + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    // generate_piece_moves(move_list, B + offset, pieces[B + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    // generate_piece_moves(move_list, R + offset, pieces[R + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    // generate_piece_moves(move_list, Q + offset, pieces[Q + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
    generate_piece_moves(move_list, K + offset, pieces[K + offset], occupancy[own_color], occupancy[enemy_color], occupancy[BOTH]);
}