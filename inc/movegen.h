#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "bitboard.h"
#include "magic.h"

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

#define NO_PIECE -1
#define MAX_PLY  256

typedef struct {
    Move  move;
    int   captured_piece; // piece index (0-11) or NO_PIECE
    int   ep_square;
    int   castle_rights;
} UndoState;

extern UndoState undo_stack[MAX_PLY];

typedef struct {
    Move moves[256];
    int count;
} MoveList;

void add_move(MoveList *move_list, Move move);

// Pawn movement helpers
Bitboard get_white_single_pushes(Bitboard wpawns, Bitboard occupied);
Bitboard get_white_double_pushes(Bitboard single_pushes, Bitboard occupied);
Bitboard get_black_single_pushes(Bitboard bpawns, Bitboard occupied);
Bitboard get_black_double_pushes(Bitboard single_pushes, Bitboard occupied);

Bitboard get_white_pawn_attack_west(Bitboard wpawns);
Bitboard get_white_pawn_attack_east(Bitboard wpawns);
Bitboard get_black_pawn_attack_west(Bitboard bpawns);
Bitboard get_black_pawn_attack_east(Bitboard bpawns);

// Leaper attack tables & initializers
extern Bitboard pawn_attacks[2][64];
extern Bitboard knight_attacks[64];
extern Bitboard king_attacks[64];
void init_leaper_attacks(void);

// Move generation functions
void generate_pawn_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq);
void generate_piece_moves(MoveList *move_list, int piece_type, Bitboard piece_bb, Bitboard own_occ, Bitboard enemy_occ, Bitboard both_occ);
void generate_castling_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int castle_rights);
void generate_all_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq, int castle_rights);

int is_square_attacked(int sq, Bitboard pieces[12], Bitboard occupancy[3], int attacker_side);

// Make / Unmake move (ply is the current search depth index into undo_stack)
int  make_move  (Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side_to_move, int *ep_square, int *castle_rights, int ply);
void unmake_move(Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side_to_move, int *ep_square, int *castle_rights, int ply);

#endif // MOVEGEN_H