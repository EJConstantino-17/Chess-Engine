// independent FEN search mode, with explicit resource limits and
// expected-UCI-move checking. No opening book is consulted.
#include "../inc/puzzle.h"
#include "../inc/search.h"
#include "../inc/tt.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(void) {
    fputs("Usage: main --puzzle \"FEN\" [--depth 15] [--time-ms 5000] "
          "[--nodes 10000000] [--expect e2e4]\n"
          "       --fen FEN is also accepted; time/nodes 0 means unlimited.\n", stderr);
}

static int decimal(const char *s, uint64_t *n) {
    char *end = NULL;
    if (!s || !*s || *s == '-') return 0;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || !end || *end) return 0;
    *n = (uint64_t)v;
    return 1;
}

// parse_fen assumes valid input and can shift outside the bitboard.
// Validate all fields before handing any untrusted command-line FEN to it.
static int validate_fen(const char *fen, int *halfmove) {
    char copy[256];
    if (!fen || strlen(fen) >= sizeof(copy)) return 0;
    strcpy(copy, fen);
    char *parts[7] = {0};
    char *token = strtok(copy, " \t\r\n");
    int n = 0;
    while (token && n < 7) { parts[n++] = token; token = strtok(NULL, " \t\r\n"); }
    if (n < 4 || n > 6) return 0;
    int rank = 7, file = 0, kings[2] = {0};
    for (const char *p = parts[0]; *p; p++) {
        if (*p == '/') {
            if (file != 8 || rank <= 0) return 0;
            rank--; file = 0;
        } else if (*p >= '1' && *p <= '8') {
            file += *p - '0';
        } else {
            if (!strchr("PNBRQKpnbrqk", *p)) return 0;
            if (*p == 'K') kings[0]++;
            if (*p == 'k') kings[1]++;
            if ((*p == 'P' || *p == 'p') && (rank == 0 || rank == 7)) return 0;
            file++;
        }
        if (file > 8) return 0;
    }
    if (rank != 0 || file != 8 || kings[0] != 1 || kings[1] != 1) return 0;
    if (strcmp(parts[1], "w") && strcmp(parts[1], "b")) return 0;
    if (strcmp(parts[2], "-")) {
        int rights = 0;
        for (const char *p = parts[2]; *p; p++) {
            const char *r = strchr("KQkq", *p);
            if (!r || (rights & (1 << (r - "KQkq")))) return 0;
            rights |= 1 << (r - "KQkq");
        }
    }
    if (strcmp(parts[3], "-")) {
        if (strlen(parts[3]) != 2 || parts[3][0] < 'a' || parts[3][0] > 'h' ||
            parts[3][1] != (parts[1][0] == 'w' ? '6' : '3')) return 0;
    }
    uint64_t h = 0, fullmove = 1;
    if (n >= 5 && (!decimal(parts[4], &h) || h > 10000)) return 0;
    if (n >= 6 && (!decimal(parts[5], &fullmove) || fullmove == 0)) return 0;
    *halfmove = (int)h;
    return 1;
}

static void move_string(Move move, char out[6]) {
    if (!move) { strcpy(out, "0000"); return; }
    out[0] = 'a' + MOVE_SRC(move) % 8;
    out[1] = '1' + MOVE_SRC(move) / 8;
    out[2] = 'a' + MOVE_TARGET(move) % 8;
    out[3] = '1' + MOVE_TARGET(move) / 8;
    int promoted = MOVE_PROMOTED(move);
    if (promoted) {
        out[4] = "pnbrqk"[promoted % 6];
        out[5] = 0;
    } else out[4] = 0;
}

static void print_line(const SearchStats *stats, const char *fen,
                       Bitboard board[12], Bitboard occupancy[3],
                       int side, int ep, int castle) {
    if (!stats->completed_depth || !stats->pv_count) {
        puts("Line: unavailable (no completed depth)");
        return;
    }
    int move_number = 1;
    sscanf(fen, "%*s %*s %*s %*s %*d %d", &move_number);
    Bitboard pieces[12], occ[3];
    memcpy(pieces, board, sizeof(pieces));
    memcpy(occ, occupancy, sizeof(occ));
    printf("Line (depth %d):", stats->completed_depth);
    for (int i = 0; i < stats->pv_count; i++) {
        Move move = stats->pv[i];
        int next_side = side, next_ep = ep, next_castle = castle;
        if (!make_move(move, pieces, occ, &next_side, &next_ep, &next_castle, i)) break;
        char text[6]; move_string(move, text);
        if (side == WHITE) printf(" %d. %s", move_number, text);
        else if (i == 0) printf(" %d... %s", move_number, text);
        else printf(" %s", text);
        if (side == BLACK) move_number++;
        side = next_side; ep = next_ep; castle = next_castle;
    }
    putchar('\n');
}

static void print_advantage(int score, int side) {
    int white_score = side == WHITE ? score : -score;
    if (score >= MATE_SCORE - MAX_PLY || score <= -MATE_SCORE + MAX_PLY) {
        int plies = MATE_SCORE - abs(score);
        printf("Advantage: %s, mate in %d\n",
               white_score > 0 ? "White" : "Black", (plies + 1) / 2);
    } else if (white_score == 0) {
        puts("Advantage: equal (0.00 pawns)");
    } else {
        printf("Advantage: %s +%.2f pawns (%+d cp from White's perspective)\n",
               white_score > 0 ? "White" : "Black",
               abs(white_score) / 100.0, white_score);
    }
}

int puzzle_cli(int argc, char **argv) {
    const char *fen = NULL, *expect = NULL;
    int depth = 15, time_ms = 5000;
    uint64_t node_limit = 10000000;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fen") && i + 1 < argc) fen = argv[++i];
        else if (!strcmp(argv[i], "--depth") && i + 1 < argc) {
            uint64_t v;
            if (!decimal(argv[++i], &v) || v < 1 || v >= MAX_PLY) { usage(); return 2; }
            depth = (int)v;
        } else if (!strcmp(argv[i], "--time-ms") && i + 1 < argc) {
            uint64_t v;
            if (!decimal(argv[++i], &v) || v > 2147483647U) { usage(); return 2; }
            time_ms = (int)v;
        } else if (!strcmp(argv[i], "--nodes") && i + 1 < argc) {
            if (!decimal(argv[++i], &node_limit)) { usage(); return 2; }
        } else if (!strcmp(argv[i], "--expect") && i + 1 < argc) expect = argv[++i];
        else if (!fen && argv[i][0] != '-') fen = argv[i];
        else { usage(); return 2; }
    }
    int halfmove = 0;
    if (!validate_fen(fen, &halfmove)) {
        fputs("Invalid FEN. Supply a quoted FEN with both kings.\n", stderr);
        usage(); return 2;
    }
    if (expect) {
        size_t len = strlen(expect);
        if (len != 4 && len != 5) { usage(); return 2; }
        for (int i = 0; i < 4; i++)
            if ((i % 2 == 0 && (expect[i] < 'a' || expect[i] > 'h')) ||
                (i % 2 == 1 && (expect[i] < '1' || expect[i] > '8'))) { usage(); return 2; }
        if (len == 5 && !strchr("qrbn", expect[4])) { usage(); return 2; }
    }
    Bitboard board[12], occupancy[3];
    int side, ep, castle;
    parse_fen(fen, board, occupancy, &side, &ep, &castle);
    SearchState history;
    search_state_reset(&history, board, side, ep, castle, halfmove);
    SearchOptions old = search_get_options(), opts = old;
    opts.max_nodes = node_limit;
    opts.max_time_ms = time_ms;
    opts.verbose = 0;
    opts.claim_draw = 0;
    opts.singular = 1;
    search_set_options(opts);
    clear_tt(); // reproducible: each puzzle starts without prior-game TT data.
    clock_t start = clock();
    Move best = search_best_move_with_state(board, occupancy, side, ep, castle, depth, &history);
    SearchStats stats = search_get_stats();
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    char uci[6]; move_string(best, uci);
    printf("Puzzle bestmove=%s score_cp=%d depth=%d/%d nodes=%" PRIu64
           " qnodes=%" PRIu64 " cpu_s=%.3f stopped=%d\n",
           uci, stats.score, stats.completed_depth, depth, stats.nodes,
           stats.qnodes, elapsed, stats.stopped);
    print_line(&stats, fen, board, occupancy, side, ep, castle);
    if (stats.completed_depth) print_advantage(stats.score, side);
    else puts("Advantage: unavailable (no completed depth)");
    printf("Singular attempted=%" PRIu64 " extended=%" PRIu64 " refuted=%" PRIu64 "\n",
           stats.singular_attempts, stats.singular_extensions, stats.singular_refutations);
    if (expect) printf("Expected=%s result=%s\n", expect,
                       !strcmp(uci, expect) ? "PASS" : "FAIL");
    search_set_options(old);
    return expect && strcmp(uci, expect) ? 1 : 0;
}
