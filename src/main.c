#include <stdio.h>
#include "moves.h"

int main(void) {
    Bitboard pieces[12] = {0};
    Bitboard occupancy[3] = {0};

    init_board(pieces, occupancy);
    init_king_attacks();
    init_knight_attacks();
    init_pawn_attacks();

    MoveList move_list;
    generate_all_moves(&move_list, pieces, occupancy, BLACK);

    printf("Total pseudo-legal moves for WHITE: %d\n", move_list.count);

    for (int i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        printf("Move %2d: Src=%2d, Target=%2d, Piece=%2d, DoublePush=%d\n",
               i + 1, MOVE_SRC(m), MOVE_TARGET(m), MOVE_PIECE(m), MOVE_IS_DOUBLE(m));
    }

    fflush(stdout);

    return 0;
}