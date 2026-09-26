#include "../inc/bitboard.h"
#include <stdio.h>

void print_bitboard(Bitboard bitboard) {
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if ((bitboard >> square) & 1ULL)
                printf(" 1 ");
            else
                printf(" . ");
        }
        printf("\n");
    }
    printf("\n");
}


void init_board(Bitboard pieces[12], Bitboard occupancy[BOTH]) {
    pieces[P]   = 0x000000000000FF00ULL;
    pieces[N] = (1ULL << B1) | (1ULL << G1);
    pieces[B] = (1ULL << C1) | (1ULL << F1);
    pieces[R]   = (1ULL << A1) | (1ULL << H1);
    pieces[Q]  = (1ULL << D1);
    pieces[K]   = (1ULL << E1);

    pieces[p]   = 0x00FF000000000000ULL;
    pieces[n] = (1ULL << B8) | (1ULL << G8);
    pieces[b] = (1ULL << C8) | (1ULL << F8);
    pieces[r]   = (1ULL << A8) | (1ULL << H8);
    pieces[q]  = (1ULL << D8);
    pieces[k]   = (1ULL << E8);

    occupancy[WHITE] = 0ULL;
    occupancy[BLACK] = 0ULL;

    for (int piece = P; piece <= K; piece++) {
        occupancy[WHITE] |= pieces[piece];
    }

    for (int piece = p; piece <= k; piece++) {
        occupancy[BLACK] |= pieces[piece];
    }

    occupancy[BOTH] = occupancy[WHITE] | occupancy[BLACK];
}

