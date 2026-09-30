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

//  Only irreversible fields; the caller retains the encoded move.
typedef struct {
    uint64_t zobrist_key;
    uint32_t halfmove_clock;
    int8_t captured_piece;
    int8_t ep_square;
    uint8_t castle_rights;
} UndoState;
#if defined(__cplusplus)
static_assert(sizeof(UndoState) == 16, "UndoState must occupy 16 bytes");
#else
_Static_assert(sizeof(UndoState) == 16, "UndoState must occupy 16 bytes");
#endif

// Live metadata is separate from the per-ply undo records.
typedef struct {
    uint64_t zobrist_key;
    uint32_t halfmove_clock;
} MoveState;

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
// No king-safety checks, including for castling candidates.
void generate_pseudo_legal_moves(MoveList *list, Bitboard pieces[12], Bitboard occupancy[3],
                                 int side, int ep, int castle);
void generate_all_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq, int castle_rights);
// Captures and promotions for qsearch outside check.
void generate_tactical_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq);

// header-local definition lets the compiler inline attack checks
// during move validation and search (with LTO it may also inline magic lookups).
static inline int is_square_attacked(int sq, Bitboard pieces[12],
                                      Bitboard occupancy[3], int attacker_side) {
    if (attacker_side == WHITE) {
        if (pawn_attacks[BLACK][sq] & pieces[P]) return 1;
    } else {
        if (pawn_attacks[WHITE][sq] & pieces[p]) return 1;
    }
    int knight_piece = attacker_side == WHITE ? N : n;
    if (knight_attacks[sq] & pieces[knight_piece]) return 1;
    int king_piece = attacker_side == WHITE ? K : k;
    if (king_attacks[sq] & pieces[king_piece]) return 1;
    int bishop_piece = attacker_side == WHITE ? B : b;
    int queen_piece = attacker_side == WHITE ? Q : q;
    if (get_bishop_attacks(sq, occupancy[BOTH]) &
        (pieces[bishop_piece] | pieces[queen_piece])) return 1;
    int rook_piece = attacker_side == WHITE ? R : r;
    if (get_rook_attacks(sq, occupancy[BOTH]) &
        (pieces[rook_piece] | pieces[queen_piece])) return 1;
    return 0;
}

// Initialize once per root/FEN, then use one slot per live ply.
void move_state_init(MoveState *state, Bitboard pieces[12], int side, int ep,
                     int castle, uint32_t halfmove);
// Success means applied, not legal. Check before NNUE commit/evaluation.
int make_move_unchecked_state(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                              int *side, int *ep, int *castle, int ply, MoveState *state);
int move_is_legal_after_make(Move move, Bitboard pieces[12], Bitboard occupancy[3]);
int make_move_state(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                    int *side, int *ep, int *castle, int ply, MoveState *state);
void unmake_move_state(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                       int *side, int *ep, int *castle, int ply, MoveState *state);
// Existing callers can continue using the board-only API.
int make_move(Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side,
              int *ep, int *castle, int ply);
void unmake_move(Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side,
                 int *ep, int *castle, int ply);

#endif
