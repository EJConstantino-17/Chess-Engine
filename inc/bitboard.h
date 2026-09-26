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

void init_board(Bitboard pieces[12], Bitboard occupancy[3]);

void print_bitboard(Bitboard bitboard);

Bitboard get_white_single_pushes(Bitboard wpawns, Bitboard occupied);
Bitboard get_white_double_pushes(Bitboard single_pushes, Bitboard occupied);

Bitboard get_black_single_pushes(Bitboard bpawns, Bitboard occupied);
Bitboard get_black_double_pushes(Bitboard single_pushes, Bitboard occupied);

Bitboard get_white_pawn_attack_west(Bitboard wpawns);
Bitboard get_white_pawn_attack_east(Bitboard wpawns);
Bitboard get_black_pawn_attack_west(Bitboard bpawns);
Bitboard get_black_pawn_attack_east(Bitboard bpawns);

extern Bitboard knight_attacks[64];
Bitboard mask_knight_attacks(int sq);
void init_knight_attacks(void);

extern Bitboard king_attacks[64];
Bitboard mask_king_attacks(int sq);
void init_king_attacks(void);

#endif