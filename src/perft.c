#include "bitboard.h"
#include "movegen.h"
#include <stdio.h>
#include <time.h>

Bitboard perft(int depth, Bitboard pieces[12], Bitboard occupancy[3],
                      int side_to_move, int ep_square, int castle_rights, int ply) {
    if (depth == 0) return 1ULL;

    Bitboard nodes = 0;
    MoveList move_list;
    generate_all_moves(&move_list, pieces, occupancy, side_to_move, ep_square, castle_rights);

    for (int i = 0; i < move_list.count; i++) {
        if (!make_move(move_list.moves[i], pieces, occupancy,
                       &side_to_move, &ep_square, &castle_rights, ply)) {
            continue;
        }

        nodes += perft(depth - 1, pieces, occupancy,
                       side_to_move, ep_square, castle_rights, ply + 1);

        unmake_move(move_list.moves[i], pieces, occupancy,
                    &side_to_move, &ep_square, &castle_rights, ply);
    }

    return nodes;
}

void run_perft_suite(int max_depth, Bitboard pieces[12], Bitboard occupancy[3]) {
    printf("RUNNING PERFT TEST (stack-based make/unmake)\n");
    for (int depth = 1; depth <= max_depth; depth++) {
        clock_t start = clock();
        Bitboard nodes = perft(depth, pieces, occupancy, WHITE, NO_SQUARE, 15, 0);
        clock_t end = clock();
        printf("Depth %d: %" PRIu64 " leaf nodes\n", depth, nodes);
        printf("Time: %.4fs\n", (double)(end - start) / CLOCKS_PER_SEC);
    }
}