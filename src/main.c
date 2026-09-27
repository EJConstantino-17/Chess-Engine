#include "eval.h"
#include "magic.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include <stdio.h>
#include <string.h>

void reset_board(Bitboard pieces[12], Bitboard occupancy[3]) {
    memset(pieces, 0, sizeof(Bitboard) * 12);
    memset(occupancy, 0, sizeof(Bitboard) * 3);
}

void update_occupancies(Bitboard pieces[12], Bitboard occupancy[3]) {
    occupancy[WHITE] = pieces[P] | pieces[N] | pieces[B] | pieces[R] | pieces[Q] | pieces[K];
    occupancy[BLACK] = pieces[p] | pieces[n] | pieces[b] | pieces[r] | pieces[q] | pieces[k];
    occupancy[BOTH]  = occupancy[WHITE] | occupancy[BLACK];
}

//sample uci parser
void square_to_uci(int square, char *output) {
    int rank = square / 8;
    int file = square % 8;

    output[0] = 'a' + file;
    output[1] = '1' + rank;
    output[2] = '\0';
}

void move_to_uci(int from_sq, int to_sq, char *move_str) {
    char from_str[3];
    char to_str[3];

    square_to_uci(from_sq, from_str);
    square_to_uci(to_sq, to_str);

    sprintf(move_str, "%s%s", from_str, to_str);
}

void init_board_main(void);

int main(void) {
    Bitboard pieces[12] = {0};
    Bitboard occupancy[3] = {0};
    int side_to_move = WHITE;
    int ep_square = NO_SQUARE;
    int castle_rights = 15;
    char best_move_str[5];
    init_board_main();

    const char *fen = "7k/3r3p/1q3P2/p1p1p1Q1/P1P5/8/5PPP/4b1K1 w - - 0 35";

    parse_fen(fen, pieces, occupancy, &side_to_move, &ep_square, &castle_rights);

    print_board(pieces);

    Move best = search_best_move(pieces, occupancy, side_to_move, ep_square, castle_rights, 7);
    move_to_uci(MOVE_SRC(best), MOVE_TARGET(best), best_move_str);
    printf("Best Move Found: %s\n", best_move_str);

    return 0;
}

void init_board_main(void) {
    init_sliders_attacks();
    init_leaper_attacks();
    init_eval_tables();
}