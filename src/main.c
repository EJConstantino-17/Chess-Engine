#include <stdio.h>
#include "moves.h"
#include "magic.h"

int main(void) {
    Bitboard pieces[12] = {0};
    Bitboard occupancy[3] = {0};

    init_board(pieces, occupancy);
    init_king_attacks();
    init_knight_attacks();
    init_pawn_attacks();
    init_sliders_attacks();

    MoveList move_list;

    int ep_sq = -1;
    int castle_rights = 15;
    generate_all_moves(&move_list, pieces, occupancy, WHITE, ep_sq, castle_rights);

    printf("Total pseudo-legal moves for WHITE: %d\n", move_list.count);

    for (int i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        printf("Move %2d: Src=%2d, Target=%2d, Piece=%2d, DoublePush=%d, Castling=%2d\n",
               i + 1, MOVE_SRC(m), MOVE_TARGET(m), MOVE_PIECE(m), MOVE_IS_DOUBLE(m), MOVE_IS_CASTLING(m));
    }

    fflush(stdout);

    printf("TESTING BISHOP AT E4 WITH BLOCKERS\n");
    Bitboard test_occupancy = (1ULL << F5) | (1ULL << D3);
    
    Bitboard b_attacks = get_bishop_attacks(E4, test_occupancy);
    print_bitboard(b_attacks);

    printf("\nTESTING ROOK AT E4 WITH BLOCKERS\n");
    test_occupancy = (1ULL << E6) | (1ULL << C4);
    
    Bitboard r_attacks = get_rook_attacks(E4, test_occupancy);
    print_bitboard(r_attacks);

    printf("\nTESTING QUEEN AT E4 WITH BLOCKERS\n");
    test_occupancy = (1ULL << E6) | (1ULL << C4) | (1ULL << F5) | (1ULL << D3);
    
    Bitboard q_attacks = get_queen_attacks(E4, test_occupancy);
    print_bitboard(q_attacks);

    printf("MY BLOCKERS MASK\n");
    print_bitboard(test_occupancy);

    printf("ON THE FLY RESULT\n");
    print_bitboard(bishop_attacks_on_the_fly(E4, test_occupancy));

    printf("MAGIC LOOKUP RESULT\n");
    print_bitboard(get_bishop_attacks(E4, test_occupancy));

    print_bitboard(occupancy[BOTH]);


    return 0;
}