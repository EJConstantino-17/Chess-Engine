#include <stdio.h>
#include "../inc/bitboard.h"

int main(void) {
    Bitboard pieces[12] = {0};
    Bitboard occupancy[3] = {0};

    init_board(pieces, occupancy);

    Bitboard white_single = get_white_single_pushes(pieces[P], occupancy[BOTH]);
    Bitboard white_double =  get_white_double_pushes(white_single, occupancy[BOTH]);
    
    Bitboard black_single = get_black_single_pushes(pieces[p], occupancy[BOTH]);
    Bitboard black_double =  get_black_double_pushes(black_single, occupancy[BOTH]);

    Bitboard white_attack_west = get_white_pawn_attack_west(pieces[P]);
    Bitboard white_attack_east = get_white_pawn_attack_east(pieces[P]);
    Bitboard white_pawn_attacks = white_attack_east | white_attack_west;

    Bitboard white_actual_captures = white_pawn_attacks & occupancy[BLACK];

    Bitboard black_attack_west = get_black_pawn_attack_west(pieces[p]);
    Bitboard black_attack_east = get_black_pawn_attack_east(pieces[p]);
    Bitboard black_pawn_attacks = black_attack_west | black_attack_east;

    Bitboard black_actual_captures = black_pawn_attacks & occupancy[WHITE];
    
    printf("Initial pawns: 0x%016" PRIx64 "\n", pieces[P] | pieces[p]);
    printf("White single: 0x%016" PRIx64 "\n", white_single);
    printf("White double: 0x%016" PRIx64 "\n", white_double);
    print_bitboard(white_double);
    printf("White attacks: 0x%016" PRIx64 "\n", white_pawn_attacks);
    print_bitboard(white_pawn_attacks);
    printf("White captures: 0x%016" PRIx64 "\n", white_actual_captures);
    printf("Black single: 0x%016" PRIx64 "\n", black_single);
    printf("Black double: 0x%016" PRIx64 "\n", black_double);
    printf("Black attacks: 0x%016" PRIx64 "\n", black_pawn_attacks);
    print_bitboard(black_pawn_attacks);
    printf("Black captures: 0x%016" PRIx64 "\n", black_actual_captures);

    print_bitboard(occupancy[BOTH]);

    init_knight_attacks();
    printf("Initial white knights: 0x%016" PRIx64 "\n", pieces[N]);
    print_bitboard(pieces[N]);
    printf("White knight attacks from g1: \n");
    print_bitboard(knight_attacks[G1]);

    init_king_attacks();
    printf("White king attacks from a4: \n");
    print_bitboard(king_attacks[A4]);
    printf("White king attacks from e4: \n");
    print_bitboard(king_attacks[E4]);
    printf("White king attacks from h4: \n");
    print_bitboard(king_attacks[H4]);
    printf("White king attacks from e1: \n");
    print_bitboard(king_attacks[E1]);
    printf("White king attacks from e8: \n");
    print_bitboard(king_attacks[E8]);

    printf("Mock chessboard: \n");
    Bitboard mock = 0x3F3F3F3F3F3F3F3FULL;
    print_bitboard(mock);

    return 0;
}