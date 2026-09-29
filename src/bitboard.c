#include "../inc/bitboard.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char piece_ascii[12] = {
    'P', 'N', 'B', 'R', 'Q', 'K',
    'p', 'n', 'b', 'r', 'q', 'k'
};

void print_board(Bitboard pieces[12]) {
    printf("\n+---+---+---+---+---+---+---+---+\n");

    for (int r = 7; r >= 0; r--) {
        for (int f = 0; f < 8; f++) {
            int square = r * 8 + f;
            int piece = -1; 

            for (int p_type = P; p_type <= k; p_type++) {
                if (TEST_BIT(pieces[p_type], square)) {
                    piece = p_type;
                    break;
                }
            }

            if (piece != -1) {
                printf("| %c ", piece_ascii[piece]);
            } else {
                printf("|   ");
            }
        }
        printf("| %d\n+---+---+---+---+---+---+---+---+\n", r + 1);
    }

    printf("  a   b   c   d   e   f   g   h\n\n");
}

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

void parse_fen(const char *fen, Bitboard pieces[12], Bitboard occupancy[3], int *side_to_move, int *ep_square, int *castle_rights) {
    memset(pieces, 0, sizeof(Bitboard) * 12);
    memset(occupancy, 0, sizeof(Bitboard) * 3);
    *side_to_move = WHITE;
    *ep_square = NO_SQUARE;
    *castle_rights = 0;
 
    int rank = 7;
    int file = 0;
 
    while (*fen && *fen != ' ') {
        if (*fen == '/') {
            rank--;
            file = 0;
        } else if (isdigit(*fen)) {
            file += (*fen - '0');
        } else {
            int sq = rank * 8 + file;
            switch (*fen) {
                case 'P': SET_BIT(pieces[P], sq); break;
                case 'N': SET_BIT(pieces[N], sq); break;
                case 'B': SET_BIT(pieces[B], sq); break;
                case 'R': SET_BIT(pieces[R], sq); break;
                case 'Q': SET_BIT(pieces[Q], sq); break;
                case 'K': SET_BIT(pieces[K], sq); break;
 
                case 'p': SET_BIT(pieces[p], sq); break;
                case 'n': SET_BIT(pieces[n], sq); break;
                case 'b': SET_BIT(pieces[b], sq); break;
                case 'r': SET_BIT(pieces[r], sq); break;
                case 'q': SET_BIT(pieces[q], sq); break;
                case 'k': SET_BIT(pieces[k], sq); break;
            }
            file++;
        }
        fen++;
    }
 
    if (*fen == ' ') fen++;
 
    if (*fen == 'w') *side_to_move = WHITE;
    else if (*fen == 'b') *side_to_move = BLACK;
    if (*fen) fen++;
 
    if (*fen == ' ') fen++;
 
    while (*fen && *fen != ' ') {
        switch (*fen) {
            case 'K': *castle_rights |= 1; break;
            case 'Q': *castle_rights |= 2; break;
            case 'k': *castle_rights |= 4; break;
            case 'q': *castle_rights |= 8; break;
            case '-': break;
        }
        fen++;
    }
 
    if (*fen == ' ') fen++;
 
    if (*fen && *fen != '-') {
        int ep_file = fen[0] - 'a';
        int ep_rank = fen[1] - '1';
        *ep_square = ep_rank * 8 + ep_file;
    }
 
    for (int p_type = P; p_type <= K; p_type++) occupancy[WHITE] |= pieces[p_type];
    for (int p_type = p; p_type <= k; p_type++) occupancy[BLACK] |= pieces[p_type];
    occupancy[BOTH] = occupancy[WHITE] | occupancy[BLACK];
}

