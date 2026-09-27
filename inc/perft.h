#ifndef PERFT_H
#define PERFT_H

#include "types.h"

Bitboard perft(int depth, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int ply);

void run_perft_suite(int max_depth, Bitboard pieces[12], Bitboard occupancy[3]);

#endif