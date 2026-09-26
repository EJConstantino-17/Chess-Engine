#include "../inc/movegen.h"

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