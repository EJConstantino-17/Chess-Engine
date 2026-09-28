#ifndef EVAL_H
#define EVAL_H

#include "bitboard.h"

void init_eval_tables(void);

int evaluate(Bitboard pieces[12], Bitboard occupancy[3], int side_to_move);

extern const int mg_value[6];

#endif