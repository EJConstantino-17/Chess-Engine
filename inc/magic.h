#ifndef MAGIC_H
#define MAGIC_H

#include "bitboard.h"

// Magic Bitboard table entry
typedef struct {
    Bitboard mask;
    Bitboard magic;
    uint32_t shift;
    Bitboard *attacks;
} MagicEntry;

extern MagicEntry bishop_magics[64];
extern MagicEntry rook_magics[64];

void init_magic_numbers(void);
void init_sliders_attacks(void);

// Mask & on-the-fly generation helpers
Bitboard mask_bishop_attacks(int sq);
Bitboard mask_rook_attacks(int sq);
Bitboard bishop_attacks_on_the_fly(int sq, Bitboard block);
Bitboard rook_attacks_on_the_fly(int sq, Bitboard block);

// Magic lookup functions
Bitboard get_bishop_attacks(int sq, Bitboard occupancy);
Bitboard get_rook_attacks(int sq, Bitboard occupancy);
Bitboard get_queen_attacks(int sq, Bitboard occupancy);

#endif // MAGIC_H