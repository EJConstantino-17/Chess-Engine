
#include "../inc/bitboard.h"
#include "../inc/magic.h"
#include "../inc/movegen.h"
#include "../inc/tt.h"
#include <stdio.h>

static uint64_t walk(Bitboard pieces[12], Bitboard occupied[3], int side,
                     int ep, int castle, unsigned depth, int ply, MoveState *state) {
    if (!depth) return 1;
    if (ply >= MAX_PLY) return 0;
    MoveList moves;
    generate_all_moves(&moves, pieces, occupied, side, ep, castle);
    uint64_t leaves = 0;
    for (int i = 0; i < moves.count; ++i) {
        Move move = moves.moves[i];
        // make stores irreversible fields in undo_stack[ply]. Rejected moves restore themselves.
        if (!make_move_state(move, pieces, occupied, &side, &ep, &castle, ply, state)) continue;
        leaves += walk(pieces, occupied, side, ep, castle, depth - 1, ply + 1, state);
        unmake_move_state(move, pieces, occupied, &side, &ep, &castle, ply, state);
    }
    return leaves;
}
int main(void) {
    Bitboard pieces[12], occupied[3]; int side, ep, castle; MoveState state;
    init_leaper_attacks(); init_sliders_attacks(); init_zobrist();
    parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
              pieces, occupied, &side, &ep, &castle);
    move_state_init(&state, pieces, side, ep, castle, 0);
    uint64_t root_key = state.zobrist_key;
    uint64_t leaves = walk(pieces, occupied, side, ep, castle, 4, 0, &state);
    printf("UndoState: %zu bytes; depth 4: %" PRIu64 " leaves\n", sizeof(UndoState), leaves);
    return leaves != 197281 || state.zobrist_key != root_key || state.halfmove_clock != 0;
}
