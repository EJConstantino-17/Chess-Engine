#include "movegen.h"
#include "magic.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

uint64_t perft(int depth, Bitboard pieces[12], Bitboard occupancy[3], int side_to_move, int ep_square, int castle_rights) {
    if (depth == 0) return 1ULL;

    uint64_t nodes = 0;
    MoveList move_list;
    generate_all_moves(&move_list, pieces, occupancy, side_to_move, ep_square, castle_rights);

    for (int i = 0; i < move_list.count; i++) {
        Bitboard pieces_copy[12];
        Bitboard occ_copy[3];
        memcpy(pieces_copy, pieces, sizeof(pieces_copy));
        memcpy(occ_copy, occupancy, sizeof(occ_copy));
        int side_copy = side_to_move;
        int ep_copy = ep_square;
        int castle_copy = castle_rights;

        if (!make_move(move_list.moves[i], pieces, occupancy, &side_to_move, &ep_square, &castle_rights)) {
            memcpy(pieces, pieces_copy, sizeof(pieces_copy));
            memcpy(occupancy, occ_copy, sizeof(occ_copy));
            side_to_move = side_copy;
            ep_square = ep_copy;
            castle_rights = castle_copy;
            continue;
        }

        nodes += perft(depth - 1, pieces, occupancy, side_to_move, ep_square, castle_rights);

        memcpy(pieces, pieces_copy, sizeof(pieces_copy));
        memcpy(occupancy, occ_copy, sizeof(occ_copy));
        side_to_move = side_copy;
        ep_square = ep_copy;
        castle_rights = castle_copy;
    }

    return nodes;
}

int main(void) {
    Bitboard pieces[12] = {0};
    Bitboard occupancy[3] = {0};

    init_board(pieces, occupancy);
    init_leaper_attacks();
    init_sliders_attacks();

    MoveList move_list;

    int ep_sq = -1;
    int castle_rights = 15;
    generate_all_moves(&move_list, pieces, occupancy, WHITE, ep_sq, castle_rights);

    clock_t start_time, end_time;
    double total_time;

    printf("RUNNING SPRINT 1 PERFT TEST\n");

    start_time = clock();
    for (int depth = 1; depth <= 6; depth++) {
        uint64_t nodes = perft(depth, pieces, occupancy, WHITE, NO_SQUARE, 15);
        printf("Depth %d: %" PRIu64 " leaf nodes\n", depth, nodes);
    }
    end_time = clock();

    total_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("Time: %.4fs", total_time);

    return 0;
}