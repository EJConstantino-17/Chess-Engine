#ifndef BITBOARD_H
#define BITBOARD_H

#include <inttypes.h>
#include <stdint.h>

typedef uint64_t Bitboard;

#define SET_BIT(bb, square) ((bb) |= (1ULL << (square)))
#define CLEAR_BIT(bb, square) ((bb) &= ~(1ULL << (square)))
#define TEST_BIT(bb, square) ((((bb) & (1ULL << (square))) != 0))

#define RANK_3_MASK 0x0000000000FF0000ULL
#define RANK_6_MASK 0x0000FF0000000000ULL

#define NOT_A_FILE 0xFEFEFEFEFEFEFEFEULL
#define NOT_AB_FILE 0xFCFCFCFCFCFCFCFCULL
#define NOT_H_FILE 0x7F7F7F7F7F7F7F7FULL
#define NOT_GH_FILE 0x3F3F3F3F3F3F3F3FULL

#define WK_RIGHT 1
#define WQ_RIGHT 2
#define BK_RIGHT 4
#define BQ_RIGHT 8

#define WK_PATH ((1ULL << F1) | (1ULL << G1))
#define WQ_PATH ((1ULL << D1) | (1ULL << C1) | (1ULL << B1))
#define BK_PATH ((1ULL << F8) | (1ULL << G8))
#define BQ_PATH ((1ULL << D8) | (1ULL << C8) | (1ULL << B8))

enum {
    P, N, B, R, Q, K,
    p, n, b, r, q, k
};
enum { WHITE, BLACK, BOTH };

enum {
    A1 = 0, B1, C1, D1, E1, F1, G1, H1,
    A2 = 8, B2, C2, D2, E2, F2, G2, H2,
    A3 = 16, B3, C3, D3, E3, F3, G3, H3,
    A4 = 24, B4, C4, D4, E4, F4, G4, H4,
    A5 = 32, B5, C5, D5, E5, F5, G5, H5,
    A6 = 40, B6, C6, D6, E6, F6, G6, H6,
    A7 = 48, B7, C7, D7, E7, F7, G7, H7,
    A8 = 56, B8, C8, D8, E8, F8, G8, H8,
};

void init_board(Bitboard pieces[12], Bitboard occupancy[BOTH]);

void print_bitboard(Bitboard bitboard);

static inline int get_lsb_index(Bitboard bitboard) {
    if (bitboard == 0) return -1;
    return __builtin_ctzll(bitboard);
}

static inline Bitboard pop_lsb(Bitboard *bitboard) {
    int index = get_lsb_index(*bitboard);
    *bitboard &= *bitboard - 1;
    return index;
}

#endif