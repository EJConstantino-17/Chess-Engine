#ifndef MAGIC_H
#define MAGIC_H

#include "bitboard.h"

void init_magic_numbers(void);
void init_sliders_attacks(void);

Bitboard mask_bishop_attacks(int sq);
Bitboard mask_rook_attacks(int sq);
Bitboard mask_bishop_attacks_on_the_fly(int sq, Bitboard block);
Bitboard mask_rook_attacks_on_the_fly(int sq, Bitboard block);

#endif