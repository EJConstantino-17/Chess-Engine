#include "movegen.h"
#include "magic.h"
#include <stdio.h>
#include <inttypes.h>
#include <time.h>

// ---------------------------------------------------------------------------
// perft — no memcpy; uses make_move/unmake_move with undo stack
// ---------------------------------------------------------------------------

static uint64_t perft(int depth, Bitboard pieces[12], Bitboard occupancy[3],
                      int side_to_move, int ep_square, int castle_rights, int ply) {
    if (depth == 0) return 1ULL;

    uint64_t nodes = 0;
    MoveList move_list;
    generate_all_moves(&move_list, pieces, occupancy, side_to_move, ep_square, castle_rights);

    for (int i = 0; i < move_list.count; i++) {
        if (!make_move(move_list.moves[i], pieces, occupancy,
                       &side_to_move, &ep_square, &castle_rights, ply)) {
            continue; // make_move already restored the board internally
        }

        nodes += perft(depth - 1, pieces, occupancy,
                       side_to_move, ep_square, castle_rights, ply + 1);

        unmake_move(move_list.moves[i], pieces, occupancy,
                    &side_to_move, &ep_square, &castle_rights, ply);
    }

    return nodes;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    Bitboard pieces[12]   = {0};
    Bitboard occupancy[3] = {0};

    init_board(pieces, occupancy);
    init_leaper_attacks();
    init_sliders_attacks();

    printf("RUNNING PERFT TEST (stack-based make/unmake)\n");

    clock_t start = clock();
    for (int depth = 1; depth <= 6; depth++) {
        uint64_t nodes = perft(depth, pieces, occupancy, WHITE, NO_SQUARE, 15, 0);
        printf("Depth %d: %" PRIu64 " leaf nodes\n", depth, nodes);
    }
    clock_t end = clock();

    printf("Time: %.4fs\n", (double)(end - start) / CLOCKS_PER_SEC);

    return 0;
}