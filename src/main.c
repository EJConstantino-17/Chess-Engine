#include "movegen.h"
#include "magic.h"
#include "perft.h"

int main(void) {
    Bitboard pieces[12]   = {0};
    Bitboard occupancy[3] = {0};

    init_board(pieces, occupancy);
    init_leaper_attacks();
    init_sliders_attacks();

    run_perft_suite(6, pieces, occupancy);
    
    return 0;
}