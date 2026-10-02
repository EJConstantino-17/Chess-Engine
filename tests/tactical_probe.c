#include "../inc/search.h"
#include "../inc/eval.h"
#include "../inc/tt.h"
#include "../nnue/cce_nnue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static void format_move(Move move, char out[6]) {
    if (!move) { strcpy(out, "0000"); return; }
    out[0] = 'a' + MOVE_SRC(move) % 8;
    out[1] = '1' + MOVE_SRC(move) / 8;
    out[2] = 'a' + MOVE_TARGET(move) % 8;
    out[3] = '1' + MOVE_TARGET(move) / 8;
    out[4] = 0;
    if (MOVE_PROMOTED(move)) {
        out[4] = "pnbrqk"[MOVE_PROMOTED(move) % 6];
        out[5] = 0;
    }
}

static int number(const char *text, int maximum) {
    char *end;
    long value = strtol(text, &end, 10);
    return *text && !*end && value >= 0 && value <= maximum ? (int)value : -1;
}

int main(int argc, char **argv) {
    if (argc < 4 || argc > 6) {
        fprintf(stderr, "Usage: tactical_probe NETWORK FEN DEPTH [MS [MODE]]\n");
        return 2;
    }
    int depth = number(argv[3], MAX_PLY - 1);
    int milliseconds = argc > 4 ? number(argv[4], 600000) : 5000;
    int mode = argc > 5 ? number(argv[5], 9) : 0;
    if (depth < 1 || milliseconds < 0 || mode < 0) return 2;
    init_sliders_attacks(); init_leaper_attacks(); init_eval_tables(); init_zobrist(); init_tt(32);
    if (!cce_nnue_load(argv[1])) {
        fprintf(stderr, "NNUE load failed: %s\n", cce_nnue_error());
        free_tt(); return 2;
    }
    Bitboard pieces[12], occupancy[3], saved[12], saved_occupancy[3];
    int side, ep, castle;
    parse_fen(argv[2], pieces, occupancy, &side, &ep, &castle);
    memcpy(saved, pieces, sizeof pieces); memcpy(saved_occupancy, occupancy, sizeof occupancy);
    SearchOptions options = search_get_options();
    options.verbose = 0; options.claim_draw = 0;
    options.max_time_ms = milliseconds; options.max_nodes = 10000000;
    if (mode == 1 || mode == 9) options.lmr = 0;
    if (mode == 2 || mode == 9) options.null_move = 0;
    if (mode == 3 || mode == 9) options.q_delta = options.q_see = 0;
    if (mode == 4 || mode == 9) options.futility = options.reverse_futility = options.razoring = 0;
    if (mode == 5 || mode == 9) options.tt_score_cutoffs = 0;
    if (mode == 6 || mode == 9) options.history_tuning = options.history_lmr = 0;
    if (mode == 7 || mode == 9) options.pvs = 0;
    if (mode == 8 || mode == 9) options.singular = 0;
    search_set_options(options);
    SearchState state;
    int halfmove = 0;
    sscanf(argv[2], "%*s %*s %*s %*s %d", &halfmove);
    search_state_reset(&state, pieces, side, ep, castle, halfmove);
    Move best = search_best_move_with_state(pieces, occupancy, side, ep, castle, depth, &state);
    SearchStats stats = search_get_stats();
    char text[6]; format_move(best, text);
    printf("mode=%d move=%s score=%d depth=%d nodes=%" PRIu64 " qnodes=%" PRIu64 " ms=%.2f stopped=%d pv=",
           mode, text, stats.score, stats.completed_depth, stats.nodes, stats.qnodes, stats.elapsed_ms, stats.stopped);
    for (int i = 0; i < stats.pv_count; ++i) {
        format_move(stats.pv[i], text); printf("%s%s", i ? " " : "", text);
    }
    puts("");
    int restored = !memcmp(pieces, saved, sizeof pieces) && !memcmp(occupancy, saved_occupancy, sizeof occupancy) &&
                   cce_nnue_validate(pieces, side);
    free_tt();
    if (!restored) { fputs("Restoration failed\n", stderr); return 1; }
    return 0;
}
