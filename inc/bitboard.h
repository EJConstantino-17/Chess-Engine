#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

// Bit manipulation helper macros
#define SET_BIT(bb, square)   ((bb) |= (1ULL << (square)))
#define CLEAR_BIT(bb, square) ((bb) &= ~(1ULL << (square)))
#define TEST_BIT(bb, square)  ((((bb) & (1ULL << (square))) != 0))

// Rank bitmasks
#define RANK_3_MASK 0x0000000000FF0000ULL
#define RANK_6_MASK 0x0000FF0000000000ULL

// File bitmasks (for pawn attack boundary checks)
#define NOT_A_FILE  0xFEFEFEFEFEFEFEFEULL
#define NOT_AB_FILE 0xFCFCFCFCFCFCFCFCULL
#define NOT_H_FILE  0x7F7F7F7F7F7F7F7FULL
#define NOT_GH_FILE 0x3F3F3F3F3F3F3F3FULL

// Castling path empty square masks
#define WK_PATH ((1ULL << F1) | (1ULL << G1))
#define WQ_PATH ((1ULL << D1) | (1ULL << C1) | (1ULL << B1))
#define BK_PATH ((1ULL << F8) | (1ULL << G8))
#define BQ_PATH ((1ULL << D8) | (1ULL << C8) | (1ULL << B8))

// Board initialization and printing utilities
void init_board(Bitboard pieces[12], Bitboard occupancy[3]);
void print_bitboard(Bitboard bitboard);

// Bit scanning and manipulation utilities
static inline int get_lsb_index(Bitboard bitboard) {
    if (bitboard == 0) return -1;
    return __builtin_ctzll(bitboard);
}

static inline Bitboard pop_lsb(Bitboard *bitboard) {
    int index = get_lsb_index(*bitboard);
    *bitboard &= *bitboard - 1;
    return index;
}

#endif // BITBOARD_H