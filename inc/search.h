#ifndef SEARCH_H
#define SEARCH_H

#include "movegen.h"

#define INFINITY_SCORE 30000
#define MATE_SCORE 29000

Move search_best_move(Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights, int depth);

#endif