#include "../inc/bitboard.h"

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