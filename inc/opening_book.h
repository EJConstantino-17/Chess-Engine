#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H
#include "search.h"
// choose a legal continuation only for an exact recorded
// start-position move sequence. Repeated lines act as weights.
Move opening_book_pick(const char *path, const SearchState *history,
                       Bitboard pieces[12], Bitboard occupancy[3],
                       int side, int ep, int castle);
// loaded distinguishes "valid book, no move" from "missing file".
Move opening_book_pick_index(const char *path, Bitboard pieces[12],
                             Bitboard occupancy[3], int side, int ep,
                             int castle, int *loaded);
#endif
