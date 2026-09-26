#include "bitboard.h"
#include <stdio.h>

void print_bitboard(Bitboard bitboard) {
    printf("+---+---+---+---+---+---+---+---+\n");

    for (int r = 7; r >= 0; r--) {
        for (int f = 0; f < 8; f++) {
            int square = r * 8 + f;
            
            if (bitboard & (1ULL << square)) {
                printf("| X ");
            } else {
                printf("|   ");
            }
        }
        printf("| %d\n+---+---+---+---+---+---+---+---+\n", r + 1);
    }

    printf("  a   b   c   d   e   f   g   h\n\n");
}


void init_board(Bitboard pieces[12], Bitboard occupancy[3]) {
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

