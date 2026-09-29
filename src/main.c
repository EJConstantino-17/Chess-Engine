#include "eval.h"
#include "magic.h"
#include "movegen.h"
#include "perft.h"
#include "../inc/search.h"
#include "tt.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include "game_loop.h"
#include "uci.h"
#include "puzzle.h"

void reset_board(Bitboard pieces[12], Bitboard occupancy[3]) {
    memset(pieces, 0, sizeof(Bitboard) * 12);
    memset(occupancy, 0, sizeof(Bitboard) * 3);
}

void update_occupancies(Bitboard pieces[12], Bitboard occupancy[3]) {
    occupancy[WHITE] = pieces[P] | pieces[N] | pieces[B] | pieces[R] | pieces[Q] | pieces[K];
    occupancy[BLACK] = pieces[p] | pieces[n] | pieces[b] | pieces[r] | pieces[q] | pieces[k];
    occupancy[BOTH]  = occupancy[WHITE] | occupancy[BLACK];
}

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

// reject negative, fractional and overflowing numeric arguments.
static int parse_play_number(const char *text, uint64_t *value) {
    if (!text || !*text) return 0;
    for (const char *p = text; *p; p++)
        if (*p < '0' || *p > '9') return 0;
    errno = 0;
    char *end;
    unsigned long long n = strtoull(text, &end, 10);
    if (errno == ERANGE || *end) return 0;
    *value = (uint64_t)n;
    return 1;
}

static void print_play_usage(void) {
    fputs("Usage: main.exe --play [--color white|black] [--depth 11] [--time-ms 2000] [--nodes 5000000]\n"
          "       --time-ms 0 or --nodes 0 disables that particular limit.\n", stderr);
}

void init_board_main(void);

int main(int argc, char **argv) {
    Bitboard pieces[12] = {0};
    Bitboard occupancy[3] = {0};
    int side_to_move = WHITE;
    int ep_square = NO_SQUARE;
    int castle_rights = 15;
    init_board_main();
    init_zobrist();
    init_tt(64);    
    // optional standalone FEN solver; no book or UCI text is emitted.
    if (argc > 1 && strcmp(argv[1], "--puzzle") == 0) {
        int result = puzzle_cli(argc - 1, argv + 1);
        free_tt();
        return result;
    }
    // GUIs launch the executable without arguments. Keep the earlier
    // terminal game available via --play; --uci remains an explicit alias.
    if (argc <= 1 || strcmp(argv[1], "--play")) {
        int result = uci_loop();
        free_tt();
        return result;
    }
    // keep an interactive turn bounded; iterative deepening
    // returns the best move from the last fully completed iteration.
    SearchOptions search_options = search_get_options();
    search_options.max_time_ms = 2000;
    search_options.max_nodes = 5000000;
    int play_depth = 11;
    int player_side = BLACK; // preserve the prior engine-as-White default.
    // only --play consumes these flags; UCI and puzzle arguments
    // continue through their own parsers. Defaults preserve old behavior.
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_play_usage();
            free_tt();
            return 0;
        }
        if (i + 1 >= argc) {
            print_play_usage();
            free_tt();
            return 2;
        }
        // the value describes the human player, so the engine
        // takes the opposite color. Validate before numeric option parsing.
        if (strcmp(argv[i], "--color") == 0) {
            if (strcmp(argv[i + 1], "white") == 0) player_side = WHITE;
            else if (strcmp(argv[i + 1], "black") == 0) player_side = BLACK;
            else {
                print_play_usage();
                free_tt();
                return 2;
            }
            i++;
            continue;
        }
        uint64_t number;
        if (!parse_play_number(argv[i + 1], &number)) {
            print_play_usage();
            free_tt();
            return 2;
        }
        if (strcmp(argv[i], "--depth") == 0 && number >= 1 && number < MAX_PLY)
            play_depth = (int)number;
        else if (strcmp(argv[i], "--time-ms") == 0 && number <= INT_MAX)
            search_options.max_time_ms = (int)number;
        else if (strcmp(argv[i], "--nodes") == 0)
            search_options.max_nodes = number;
        else {
            print_play_usage();
            free_tt();
            return 2;
        }
        i++;
    }
    search_set_options(search_options);
    printf("Terminal search limits: depth %d, time %d ms, nodes %" PRIu64 "\n",
           play_depth, search_options.max_time_ms, search_options.max_nodes);
    printf("Player: %s | Engine: %s\n",
           player_side == WHITE ? "White" : "Black",
           player_side == WHITE ? "Black" : "White");
    const char *fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0";

    parse_fen(fen, pieces, occupancy, &side_to_move, &ep_square, &castle_rights);
    // parse FEN's fifth field; parse_fen currently discards it.
    int halfmove_clock = 0;
    if (sscanf(fen, "%*s %*s %*s %*s %d", &halfmove_clock) != 1) halfmove_clock = 0;
    SearchState game_history;
    search_state_reset(&game_history, pieces, side_to_move,
                       ep_square, castle_rights, halfmove_clock);

    play_game_loop_with_state(pieces, occupancy, side_to_move, ep_square, castle_rights, player_side ^ 1, play_depth, &game_history);

    free_tt();
    return 0;
}

void init_board_main(void) {
    init_sliders_attacks();
    init_leaper_attacks();
    init_eval_tables();
}
