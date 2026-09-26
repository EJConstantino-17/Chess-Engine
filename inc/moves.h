#ifndef MOVES_H
#define MOVES_H

#include <stdint.h>
#include "movegen.h"

/*
  Move Bitfield Structure (32-bit uint32_t):
  0000 0000 0000 0000 0000 0000 0011 1111 : Source Square (0-63)      -> 6 bits
  0000 0000 0000 0000 0000 1111 1100 0000 : Target Square (0-63)      -> 6 bits
  0000 0000 0000 0000 1111 0000 0000 0000 : Moved Piece (0-11)        -> 4 bits
  0000 0000 0000 1111 0000 0000 0000 0000 : Promoted Piece (0-11)     -> 4 bits
  0000 0000 0001 0000 0000 0000 0000 0000 : Capture Flag              -> 1 bit
  0000 0000 0010 0000 0000 0000 0000 0000 : Double Push Flag          -> 1 bit
  0000 0000 0100 0000 0000 0000 0000 0000 : En Passant Flag           -> 1 bit
  0000 0000 1000 0000 0000 0000 0000 0000 : Castling Flag             -> 1 bit
*/

typedef uint32_t Move;

#define ENCODE_MOVE(src, target, piece, promoted, capture, double_push, en_passant, castling) \
    ((src) |                                    \
    ((target) << 6) |                           \
    ((piece) << 12) |                           \
    ((promoted) << 16) |                        \
    ((capture) << 20) |                         \
    ((double_push) << 21) |                     \
    ((en_passant) << 22) |                      \
    ((castling) << 23))

#define MOVE_SRC(move)          ((move) & 0x3F)
#define MOVE_TARGET(move)       (((move) >> 6) & 0x3F)
#define MOVE_PIECE(move)        (((move) >> 12) & 0x0F)
#define MOVE_PROMOTED(move)     (((move) >> 16) & 0x0F)
#define MOVE_IS_CAPTURE(move)   (((move) >> 20) & 0x01)
#define MOVE_IS_DOUBLE(move)    (((move) >> 21) & 0x01)
#define MOVE_IS_EP(move)        (((move) >> 22) & 0x01)
#define MOVE_IS_CASTLING(move)  (((move) >> 23) & 0x01)

struct MoveList {
    Move moves[256];
    int count;
};

void add_move(MoveList *move_list, Move move);

#endif