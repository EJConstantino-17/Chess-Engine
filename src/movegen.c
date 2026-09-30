#include "../nnue/cce_nnue.h"
#include "../inc/movegen.h"
#include "../inc/magic.h"
#include "../inc/tt.h"

// Global undo stack (indexed by ply)
UndoState undo_stack[MAX_PLY];

void add_move(MoveList *move_list, Move move) {
    // protect the fixed move list even for malformed FEN positions.
    if (move_list->count >= (int)(sizeof(move_list->moves) / sizeof(move_list->moves[0]))) return;
    move_list->moves[move_list->count] = move;
    move_list->count++;
}


Bitboard get_white_single_pushes(Bitboard wpawns, Bitboard occupied) {
    return (wpawns << 8) & ~occupied;
}

Bitboard get_white_double_pushes(Bitboard single_pushes, Bitboard occupied) {
    return ((single_pushes & RANK_3_MASK) << 8) & ~occupied;
}

Bitboard get_black_single_pushes(Bitboard bpawns, Bitboard occupied) {
    return (bpawns >> 8) & ~occupied;
}

Bitboard get_black_double_pushes(Bitboard single_pushes, Bitboard occupied) {
    return ((single_pushes & RANK_6_MASK) >> 8) & ~occupied;
}

Bitboard get_white_pawn_attack_west(Bitboard wpawns) { return (wpawns & NOT_A_FILE) << 7; }
Bitboard get_white_pawn_attack_east(Bitboard wpawns) { return (wpawns & NOT_H_FILE) << 9; }
Bitboard get_black_pawn_attack_west(Bitboard bpawns) { return (bpawns & NOT_A_FILE) >> 9; }
Bitboard get_black_pawn_attack_east(Bitboard bpawns) { return (bpawns & NOT_H_FILE) >> 7; }


static Bitboard mask_knight_attacks(int sq) {
    Bitboard bb = (1ULL << sq);
    Bitboard a = 0ULL;
    a |= (bb << 15) & NOT_H_FILE;
    a |= (bb << 17) & NOT_A_FILE;
    a |= (bb << 6)  & NOT_GH_FILE;
    a |= (bb << 10) & NOT_AB_FILE;
    a |= (bb >> 15) & NOT_A_FILE;
    a |= (bb >> 17) & NOT_H_FILE;
    a |= (bb >> 6)  & NOT_AB_FILE;
    a |= (bb >> 10) & NOT_GH_FILE;
    return a;
}

static Bitboard mask_king_attacks(int sq) {
    Bitboard bb = (1ULL << sq);
    Bitboard a = 0ULL;
    a |= (bb << 1) & NOT_A_FILE;
    a |= (bb << 7) & NOT_H_FILE;
    a |= (bb << 9) & NOT_A_FILE;
    a |= (bb >> 1) & NOT_H_FILE;
    a |= (bb >> 7) & NOT_A_FILE;
    a |= (bb >> 9) & NOT_H_FILE;
    a |=  bb << 8;
    a |=  bb >> 8;
    return a;
}

static Bitboard mask_pawn_attacks(int side, int sq) {
    Bitboard bb = (1ULL << sq);
    Bitboard a = 0ULL;
    if (side == WHITE) {
        a |= (bb << 7) & NOT_H_FILE;
        a |= (bb << 9) & NOT_A_FILE;
    } else {
        a |= (bb >> 7) & NOT_A_FILE;
        a |= (bb >> 9) & NOT_H_FILE;
    }
    return a;
}

Bitboard pawn_attacks[2][64];
Bitboard knight_attacks[64];
Bitboard king_attacks[64];

void init_leaper_attacks(void) {
    for (int sq = 0; sq < 64; sq++) {
        pawn_attacks[WHITE][sq] = mask_pawn_attacks(WHITE, sq);
        pawn_attacks[BLACK][sq] = mask_pawn_attacks(BLACK, sq);
        knight_attacks[sq]      = mask_knight_attacks(sq);
        king_attacks[sq]        = mask_king_attacks(sq);
    }
}


void generate_pawn_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq) {
    if (side_to_move == WHITE) {
        Bitboard wpawns = pieces[P];

        while (wpawns) {
            int src = pop_lsb(&wpawns);

            Bitboard single_push = ((1ULL << src) << 8) & ~occupancy[BOTH];
            if (single_push) {
                int target = get_lsb_index(single_push);

                if (target >= A8 && target <= H8) {
                    add_move(move_list, ENCODE_MOVE(src, target, P, Q, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, R, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, B, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, N, 0, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, P, 0, 0, 0, 0, 0));

                    Bitboard double_push = ((1ULL << target) << 8) & ~occupancy[BOTH];
                    if ((1ULL << target) & RANK_3_MASK && double_push) {
                        int double_target = get_lsb_index(double_push);
                        add_move(move_list, ENCODE_MOVE(src, double_target, P, 0, 0, 1, 0, 0));
                    }
                }
            }

            // kings are never captured; checkmate ends the game.
            Bitboard attacks = pawn_attacks[WHITE][src] & (occupancy[BLACK] & ~pieces[k]);
            while (attacks) {
                int target = pop_lsb(&attacks);
                if (target >= A8 && target <= H8) {
                    add_move(move_list, ENCODE_MOVE(src, target, P, Q, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, R, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, B, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, P, N, 1, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, P, 0, 1, 0, 0, 0));
                }
            }
        }

        if (ep_sq != NO_SQUARE) {
            Bitboard attackers = pawn_attacks[BLACK][ep_sq] & pieces[P];
            while (attackers) {
                int src = pop_lsb(&attackers);
                add_move(move_list, ENCODE_MOVE(src, ep_sq, P, 0, 1, 0, 1, 0));
            }
        }
    } else {
        Bitboard bpawns = pieces[p];

        while (bpawns) {
            int src = pop_lsb(&bpawns);

            Bitboard single_push = ((1ULL << src) >> 8) & ~occupancy[BOTH];
            if (single_push) {
                int target = get_lsb_index(single_push);

                if (target >= A1 && target <= H1) {
                    add_move(move_list, ENCODE_MOVE(src, target, p, q, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, r, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, b, 0, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, n, 0, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, p, 0, 0, 0, 0, 0));

                    Bitboard double_push = ((1ULL << target) >> 8) & ~occupancy[BOTH];
                    if ((1ULL << target) & RANK_6_MASK && double_push) {
                        int double_target = get_lsb_index(double_push);
                        add_move(move_list, ENCODE_MOVE(src, double_target, p, 0, 0, 1, 0, 0));
                    }
                }
            }

            Bitboard attacks = pawn_attacks[BLACK][src] & (occupancy[WHITE] & ~pieces[K]);
            while (attacks) {
                int target = pop_lsb(&attacks);
                if (target >= A1 && target <= H1) {
                    add_move(move_list, ENCODE_MOVE(src, target, p, q, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, r, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, b, 1, 0, 0, 0));
                    add_move(move_list, ENCODE_MOVE(src, target, p, n, 1, 0, 0, 0));
                } else {
                    add_move(move_list, ENCODE_MOVE(src, target, p, 0, 1, 0, 0, 0));
                }
            }
        }

        if (ep_sq != NO_SQUARE) {
            Bitboard attackers = pawn_attacks[WHITE][ep_sq] & pieces[p];
            while (attackers) {
                int src = pop_lsb(&attackers);
                add_move(move_list, ENCODE_MOVE(src, ep_sq, p, 0, 1, 0, 1, 0));
            }
        }
    }
}

void generate_piece_moves(MoveList *move_list, int piece_type, Bitboard piece_bb, Bitboard own_occ, Bitboard enemy_occ, Bitboard both_occ) {
    while (piece_bb) {
        int src = pop_lsb(&piece_bb);
        Bitboard attacks = 0ULL;
        switch (piece_type) {
            case N: case n: attacks = knight_attacks[src]; break;
            case B: case b: attacks = get_bishop_attacks(src, both_occ); break;
            case R: case r: attacks = get_rook_attacks(src, both_occ); break;
            case Q: case q: attacks = get_queen_attacks(src, both_occ); break;
            case K: case k: attacks = king_attacks[src]; break;
        }

        // the excluded occupied square is the opposing king.
        attacks &= ~own_occ & ~(both_occ & ~own_occ & ~enemy_occ);

        while (attacks) {
            int target = pop_lsb(&attacks);
            int is_capture = (enemy_occ & (1ULL << target)) ? 1 : 0;
            add_move(move_list, ENCODE_MOVE(src, target, piece_type, 0, is_capture, 0, 0, 0));
        }
    }
}

// Geometry and occupancy only; search validates played moves.
void generate_pseudo_legal_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq, int castle_rights) {
    move_list->count = 0;

    int own_color   = side_to_move;
    int enemy_color = (side_to_move == WHITE) ? BLACK : WHITE;
    // keep the enemy king as a blocker but exclude it as a capture target.
    Bitboard capturable_enemy = occupancy[enemy_color] & ~pieces[side_to_move == WHITE ? k : K];

    generate_pawn_moves(move_list, pieces, occupancy, side_to_move, ep_sq);
    int offset = (side_to_move == WHITE) ? 0 : 6;
    generate_piece_moves(move_list, N + offset, pieces[N + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, B + offset, pieces[B + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, R + offset, pieces[R + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, Q + offset, pieces[Q + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);
    generate_piece_moves(move_list, K + offset, pieces[K + offset], occupancy[own_color], capturable_enemy, occupancy[BOTH]);

    generate_castling_moves(move_list, pieces, occupancy, side_to_move, castle_rights);
}

// Retain the public name used by game, book, and perft callers.
void generate_all_moves(MoveList *list, Bitboard pieces[12], Bitboard occupancy[3],
                        int side, int ep, int castle) {
    generate_pseudo_legal_moves(list, pieces, occupancy, side, ep, castle);
}

// Avoid generating quiet piece moves and castling at q-nodes.
void generate_tactical_moves(MoveList *move_list, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_sq) {
    move_list->count = 0;
    generate_pawn_moves(move_list, pieces, occupancy, side_to_move, ep_sq);
    int count = 0;
    for (int i = 0; i < move_list->count; i++) {
        Move m = move_list->moves[i];
        if (MOVE_IS_CAPTURE(m) || MOVE_PROMOTED(m)) move_list->moves[count++] = m;
    }
    move_list->count = count;
    int offset = side_to_move == WHITE ? 0 : 6;
    Bitboard enemy = occupancy[side_to_move ^ 1] & ~pieces[side_to_move == WHITE ? k : K];
    for (int type = N; type <= K; type++) {
        if (type == P) continue;
        Bitboard active = pieces[type + offset];
        while (active) {
            int src = pop_lsb(&active);
            Bitboard attacks = 0;
            switch (type) {
                case N: attacks = knight_attacks[src]; break;
                case B: attacks = get_bishop_attacks(src, occupancy[BOTH]); break;
                case R: attacks = get_rook_attacks(src, occupancy[BOTH]); break;
                case Q: attacks = get_queen_attacks(src, occupancy[BOTH]); break;
                case K: attacks = king_attacks[src]; break;
            }
            attacks &= enemy;
            while (attacks) {
                int dst = pop_lsb(&attacks);
                add_move(move_list, ENCODE_MOVE(src, dst, type + offset, 0, 1, 0, 0, 0));
            }
        }
    }
}

// Castling candidates need rights, king/rook presence, and empty paths.
void generate_castling_moves(MoveList *list, Bitboard pieces[12], Bitboard occupancy[3],
                             int side, int rights) {
    if (side == WHITE) {
        if ((rights & WK_RIGHT) && TEST_BIT(pieces[K], E1) && TEST_BIT(pieces[R], H1) &&
            !(occupancy[BOTH] & WK_PATH)) add_move(list, ENCODE_MOVE(E1,G1,K,0,0,0,0,1));
        if ((rights & WQ_RIGHT) && TEST_BIT(pieces[K], E1) && TEST_BIT(pieces[R], A1) &&
            !(occupancy[BOTH] & WQ_PATH)) add_move(list, ENCODE_MOVE(E1,C1,K,0,0,0,0,1));
    } else {
        if ((rights & BK_RIGHT) && TEST_BIT(pieces[k], E8) && TEST_BIT(pieces[r], H8) &&
            !(occupancy[BOTH] & BK_PATH)) add_move(list, ENCODE_MOVE(E8,G8,k,0,0,0,0,1));
        if ((rights & BQ_RIGHT) && TEST_BIT(pieces[k], E8) && TEST_BIT(pieces[r], A8) &&
            !(occupancy[BOTH] & BQ_PATH)) add_move(list, ENCODE_MOVE(E8,C8,k,0,0,0,0,1));
    }
}

static const int castling_rights_update[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11
};


// A square toggle updates a piece, its side, BOTH, and the live hash.
static inline void toggle_piece(Bitboard pieces[12], Bitboard occupancy[3],
                                int piece, Bitboard squares, MoveState *state) {
    pieces[piece] ^= squares;
    occupancy[piece <= K ? WHITE : BLACK] ^= squares;
    occupancy[BOTH] ^= squares;
    if (state) {
        while (squares) {
            int sq = pop_lsb(&squares);
            state->zobrist_key ^= z_pieces[piece][sq];
        }
    }
}

void move_state_init(MoveState *state, Bitboard pieces[12], int side, int ep,
                     int castle, uint32_t halfmove) {
    state->zobrist_key = generate_zobrist_key(pieces, side, ep, castle);
    state->halfmove_clock = halfmove;
}

static Bitboard castle_rook_squares(int target) {
    int from = target == G1 ? H1 : target == C1 ? A1 : target == G8 ? H8 : A8;
    int to = target == G1 ? F1 : target == C1 ? D1 : target == G8 ? F8 : D8;
    return (1ULL << from) | (1ULL << to);
}

// Apply a generated candidate; NNUE is prepared but not committed.
int make_move_unchecked_state(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                    int *side, int *ep, int *castle, int ply, MoveState *state) {
    int src = MOVE_SRC(move), dst = MOVE_TARGET(move), piece = MOVE_PIECE(move);
    int promoted = MOVE_PROMOTED(move), capture = MOVE_IS_CAPTURE(move);
    int is_ep = MOVE_IS_EP(move), is_castle = MOVE_IS_CASTLING(move);
    int us = *side, them = us ^ 1, captured = NO_PIECE;
    int cap_sq = is_ep ? dst + (us == WHITE ? -8 : 8) : dst;
    Bitboard from = 1ULL << src, to = 1ULL << dst;
    // Validate XOR preconditions before changing any board or NNUE state.
    if (ply < 0 || ply >= MAX_PLY || piece > k || src == dst ||
        (piece <= K ? WHITE : BLACK) != us || !(pieces[piece] & from) ||
        (occupancy[us] & to) || (pieces[them == WHITE ? K : k] & to)) return 0;
    if (promoted && (promoted > k || (promoted <= K ? WHITE : BLACK) != us ||
        (promoted % 6 < N || promoted % 6 > Q) || piece % 6 != P ||
        dst / 8 != (us == WHITE ? 7 : 0))) return 0;
    if (piece % 6 == P && dst / 8 == (us == WHITE ? 7 : 0) && !promoted) return 0;
    if (is_ep) {
        if (!capture || piece % 6 != P || dst != *ep || cap_sq < 0 || cap_sq >= 64 ||
            occupancy[BOTH] & to || !(pawn_attacks[us][src] & to)) return 0;
        captured = them == WHITE ? P : p;
        if (!(pieces[captured] & (1ULL << cap_sq))) return 0;
    } else if (capture) {
        for (int pt = them == WHITE ? P : p; pt <= (them == WHITE ? Q : q); ++pt)
            if (pieces[pt] & to) { captured = pt; break; }
        if (captured == NO_PIECE) return 0;
    } else if (occupancy[them] & to) return 0;
    if (is_castle) {
        int valid_target = us == WHITE ? (dst == G1 || dst == C1) : (dst == G8 || dst == C8);
        int rook_from = dst == G1 ? H1 : dst == C1 ? A1 : dst == G8 ? H8 : A8;
        int right = dst == G1 ? 1 : dst == C1 ? 2 : dst == G8 ? 4 : 8;
        Bitboard path = dst == G1 ? ((1ULL << F1) | (1ULL << G1)) :
                        dst == C1 ? ((1ULL << B1) | (1ULL << C1) | (1ULL << D1)) :
                        dst == G8 ? ((1ULL << F8) | (1ULL << G8)) :
                                    ((1ULL << B8) | (1ULL << C8) | (1ULL << D8));
        if (!valid_target || piece != (us == WHITE ? K : k) || src != (us == WHITE ? E1 : E8) ||
            capture || promoted || is_ep || MOVE_IS_DOUBLE(move) || !(*castle & right) ||
            !(pieces[us == WHITE ? R : r] & (1ULL << rook_from)) || occupancy[BOTH] & path) return 0;
    }
    if (MOVE_IS_DOUBLE(move) && (piece % 6 != P || capture || promoted || is_ep ||
        src / 8 != (us == WHITE ? 1 : 6) || dst != src + (us == WHITE ? 16 : -16) ||
        (occupancy[BOTH] & (1ULL << (src + (us == WHITE ? 8 : -8)))))) return 0;

    cce_nnue_prepare(move, pieces, us, *ep, *castle, ply);
    UndoState *undo = &undo_stack[ply];
    undo->captured_piece = (int8_t)captured;
    undo->ep_square = (int8_t)*ep;
    undo->castle_rights = (uint8_t)*castle;
    undo->zobrist_key = state ? state->zobrist_key : 0;
    undo->halfmove_clock = state ? state->halfmove_clock : 0;
    if (state) state->zobrist_key ^= zobrist_ep_key(pieces, us, *ep) ^ z_castle[*castle & 15];

    if (captured != NO_PIECE) toggle_piece(pieces, occupancy, captured, 1ULL << cap_sq, state);
    toggle_piece(pieces, occupancy, piece, from | to, state);
    if (promoted) {
        toggle_piece(pieces, occupancy, piece, to, state);
        toggle_piece(pieces, occupancy, promoted, to, state);
    }
    if (is_castle) toggle_piece(pieces, occupancy, us == WHITE ? R : r, castle_rook_squares(dst), state);
    *castle &= castling_rights_update[src] & castling_rights_update[dst];
    *ep = MOVE_IS_DOUBLE(move) ? dst + (us == WHITE ? -8 : 8) : NO_SQUARE;
    *side = them;
    if (state) {
        state->zobrist_key ^= z_side ^ z_castle[*castle & 15] ^ zobrist_ep_key(pieces, them, *ep);
        if (piece % 6 == P || captured != NO_PIECE) state->halfmove_clock = 0;
        else if (state->halfmove_clock < UINT32_MAX) ++state->halfmove_clock;
    }
    return 1;
}

// Check the mover, not the opponent whose turn it now is.
int move_is_legal_after_make(Move move, Bitboard pieces[12], Bitboard occupancy[3]) {
    int piece = MOVE_PIECE(move), us = piece <= K ? WHITE : BLACK, them = us ^ 1;
    int king = us == WHITE ? K : k;
    int king_sq = get_lsb_index(pieces[king]);
    if (king_sq < 0 || is_square_attacked(king_sq, pieces, occupancy, them)) return 0;
    if (!MOVE_IS_CASTLING(move)) return 1;

    // Castling also forbids an attacked starting or transit square. Temporarily
    // reconstruct those occupancies; no callbacks, hash updates, or NNUE commits occur here.
    int src = MOVE_SRC(move), dst = MOVE_TARGET(move);
    int transit = dst > src ? src + 1 : src - 1;
    int rook = us == WHITE ? R : r;
    Bitboard king_delta = (1ULL << src) | (1ULL << dst);
    Bitboard rook_delta = castle_rook_squares(dst);
    toggle_piece(pieces, occupancy, king, king_delta, NULL);
    toggle_piece(pieces, occupancy, rook, rook_delta, NULL);
    int safe = !is_square_attacked(src, pieces, occupancy, them);
    Bitboard transit_delta = (1ULL << src) | (1ULL << transit);
    if (safe) {
        toggle_piece(pieces, occupancy, king, transit_delta, NULL);
        safe = !is_square_attacked(transit, pieces, occupancy, them);
        toggle_piece(pieces, occupancy, king, transit_delta, NULL);
    }
    toggle_piece(pieces, occupancy, rook, rook_delta, NULL);
    toggle_piece(pieces, occupancy, king, king_delta, NULL);
    return safe;
}

// Compatibility API still rejects and rolls back illegal moves.
int make_move_state(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                    int *side, int *ep, int *castle, int ply, MoveState *state) {
    if (!make_move_unchecked_state(move, pieces, occupancy, side, ep, castle, ply, state)) return 0;
    if (!move_is_legal_after_make(move, pieces, occupancy)) {
        unmake_move_state(move, pieces, occupancy, side, ep, castle, ply, state);
        return 0;
    }
    cce_nnue_commit(move, ply);
    return 1;
}

void unmake_move_state(Move move, Bitboard pieces[12], Bitboard occupancy[3],
                       int *side, int *ep, int *castle, int ply, MoveState *state) {
    cce_nnue_unmake(move, ply);
    int src = MOVE_SRC(move), dst = MOVE_TARGET(move), piece = MOVE_PIECE(move);
    int promoted = MOVE_PROMOTED(move), us = piece <= K ? WHITE : BLACK;
    Bitboard to = 1ULL << dst;
    const UndoState *undo = &undo_stack[ply];
    if (MOVE_IS_CASTLING(move)) toggle_piece(pieces, occupancy, us == WHITE ? R : r, castle_rook_squares(dst), NULL);
    if (promoted) {
        toggle_piece(pieces, occupancy, promoted, to, NULL);
        toggle_piece(pieces, occupancy, piece, to, NULL);
    }
    toggle_piece(pieces, occupancy, piece, (1ULL << src) | to, NULL);
    if (undo->captured_piece != NO_PIECE) {
        int cap_sq = MOVE_IS_EP(move) ? dst + (us == WHITE ? -8 : 8) : dst;
        toggle_piece(pieces, occupancy, undo->captured_piece, 1ULL << cap_sq, NULL);
    }
    *side = us;
    *ep = undo->ep_square;
    *castle = undo->castle_rights;
    // Restore the exact prior hash and clock instead of rehashing the board.
    if (state) {
        state->zobrist_key = undo->zobrist_key;
        state->halfmove_clock = undo->halfmove_clock;
    }
}

int make_move(Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side,
              int *ep, int *castle, int ply) {
    return make_move_state(move, pieces, occupancy, side, ep, castle, ply, NULL);
}
void unmake_move(Move move, Bitboard pieces[12], Bitboard occupancy[3], int *side,
                 int *ep, int *castle, int ply) {
    unmake_move_state(move, pieces, occupancy, side, ep, castle, ply, NULL);
}
