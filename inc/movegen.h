#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "../inc/bitboard.h"

Bitboard get_white_single_pushes(Bitboard wpawns, Bitboard occupied);
Bitboard get_white_double_pushes(Bitboard single_pushes, Bitboard occupied);

Bitboard get_black_single_pushes(Bitboard bpawns, Bitboard occupied);
Bitboard get_black_double_pushes(Bitboard single_pushes, Bitboard occupied);

Bitboard get_white_pawn_attack_west(Bitboard wpawns);
Bitboard get_white_pawn_attack_east(Bitboard wpawns);
Bitboard get_black_pawn_attack_west(Bitboard bpawns);
Bitboard get_black_pawn_attack_east(Bitboard bpawns);

extern Bitboard knight_attacks[64];
void init_knight_attacks(void);

extern Bitboard king_attacks[64];
void init_king_attacks(void);

extern Bitboard pawn_attacks[2][64];
void init_pawn_attacks(void);

typedef struct {
    Bitboard mask;
    Bitboard magic;
    uint32_t shift;
    Bitboard *attacks;
} MagicEntry;

extern MagicEntry bishop_magics[64];
extern MagicEntry rook_magics[64];

Bitboard get_bishop_attacks(int sq, Bitboard occupied);
Bitboard get_rook_attacks(int sq, Bitboard occupied);
Bitboard get_queen_attacks(int sq, Bitboard occupied);

#endif