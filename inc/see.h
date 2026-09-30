#ifndef CCE_SEE_H
#define CCE_SEE_H
#include "movegen.h"
int see_piece_value(int piece);
int see_after_capture(Move move, const Bitboard pieces[12], const Bitboard occupancy[3], int victim);
#endif
