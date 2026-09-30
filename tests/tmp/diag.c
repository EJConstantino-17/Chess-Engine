// Build separately from src/main.c:
// From engine_work: gcc -O2 -std=c11 -Iinc tests/engine_diagnostics.c
//   src/bitboard.c src/eval.c src/magic.c src/movegen.c src/perft.c
//   src/search.c src/tt.c -o engine_diagnostics
// Run ./engine_diagnostics [--quick | --stress] [--depth N]
#include "../../inc/bitboard.h"
#include "../../inc/eval.h"
#include "../../inc/magic.h"
#include "../../inc/movegen.h"
#include "../../inc/perft.h"
#include "../../inc/search.h"
#include "../../inc/tt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures;
static void verdict(const char *name, int ok) {
    printf("%-42s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}
static void position(const char *fen, Bitboard b[12], Bitboard o[3],
                     int *side, int *ep, int *castle) {
    parse_fen(fen, b, o, side, ep, castle);
}
static Move find_move(Bitboard b[12], Bitboard o[3], int side, int ep,
                      int castle, int src, int dest) {
    MoveList moves;
    generate_all_moves(&moves, b, o, side, ep, castle);
    for (int i = 0; i < moves.count; i++)
        if (MOVE_SRC(moves.moves[i]) == (unsigned)src &&
            MOVE_TARGET(moves.moves[i]) == (unsigned)dest) return moves.moves[i];
    return 0;
}

// SEARCH UNDO REGRESSION: every legal and rejected move restores all board
// arrays and metadata, including promotions, en passant, and castling.
static int move_roundtrips(const char *fen) {
    Bitboard b[12], o[3], original[12], occ_original[3];
    int side, ep, castle;
    position(fen,b,o,&side,&ep,&castle);
    memcpy(original,b,sizeof(original));
    memcpy(occ_original,o,sizeof(occ_original));
    MoveList moves;
    generate_all_moves(&moves,b,o,side,ep,castle);
    int ok = moves.count > 0;
    for(int i=0;i<moves.count;i++) {
        int s=side,e=ep,c=castle;
        if(make_move(moves.moves[i],b,o,&s,&e,&c,0)) {
            Bitboard white=0,black=0;
            for(int j=P;j<=K;j++) white |= b[j];
            for(int j=p;j<=k;j++) black |= b[j];
            if(o[WHITE]!=white || o[BLACK]!=black || o[BOTH]!=(white|black)) ok=0;
            unmake_move(moves.moves[i],b,o,&s,&e,&c,0);
        }
        if(memcmp(original,b,sizeof(original)) || memcmp(occ_original,o,sizeof(occ_original)) ||
           s!=side || e!=ep || c!=castle) ok=0;
    }
    return ok;
}

static void correctness_suite(void) {
    Bitboard b[12], o[3]; int side, ep, castle;
    puts("\nCORRECTNESS CHECKS");
    position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
             b, o, &side, &ep, &castle);
    verdict("Start evaluation is 0", evaluate(b, o, side) == 0);
    verdict("Perft depth 1..4: 20/400/8902/197281",
            perft(1,b,o,side,ep,castle,0)==20 &&
            perft(2,b,o,side,ep,castle,0)==400 &&
            perft(3,b,o,side,ep,castle,0)==8902 &&
            perft(4,b,o,side,ep,castle,0)==197281);
    verdict("Make/unmake castling occupancy",
            move_roundtrips("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    verdict("Make/unmake en passant occupancy",
            move_roundtrips("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"));
    verdict("Make/unmake promotion occupancy",
            move_roundtrips("4k3/P7/8/8/8/8/7p/4K3 w - - 0 1"));
    position("7k/8/5K1Q/8/8/8/8/8 w - - 0 1",b,o,&side,&ep,&castle);
    verdict("Never generate a king capture", !find_move(b,o,side,ep,castle,H6,H8));
    Move invalid = ENCODE_MOVE(H6,H8,Q,0,1,0,0,0);
    verdict("Reject a direct king capture", !make_move(invalid,b,o,&side,&ep,&castle,0));
    position("4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1",b,o,&side,&ep,&castle);
    verdict("Castling needs a rook", !find_move(b,o,side,ep,castle,E1,G1));
    position("7k/6Q1/5K2/8/8/8/8/8 b - - 0 1",b,o,&side,&ep,&castle);
    SearchState state;
    search_state_reset(&state,b,side,ep,castle,100);
    verdict("Checkmate takes precedence at halfmove 100",
            !search_draw_claim_available(b,o,side,ep,castle,&state));
    position("k7/8/8/8/8/8/8/7K w - - 100 1",b,o,&side,&ep,&castle);
    search_state_reset(&state,b,side,ep,castle,100);
    verdict("Fifty-move claim at 100 halfmoves",
            search_draw_claim_available(b,o,side,ep,castle,&state));
    position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
             b,o,&side,&ep,&castle);
    search_state_reset(&state,b,side,ep,castle,0);
    int src[8]={G1,G8,F3,F6,G1,G8,F3,F6};
    int dest[8]={F3,F6,G1,G8,F3,F6,G1,G8};
    int valid=1, second_draw=0;
    for (int i=0;i<8;i++) {
        Move m=find_move(b,o,side,ep,castle,src[i],dest[i]);
        if (!m || !make_move(m,b,o,&side,&ep,&castle,0)) { valid=0; break; }
        search_state_record_move(&state,m,b,side,ep,castle);
        if (i==3) second_draw=search_draw_claim_available(b,o,side,ep,castle,&state);
    }
    verdict("Third appearance, not second, is a draw",
            valid && !second_draw &&
            search_draw_claim_available(b,o,side,ep,castle,&state));
    position("4k3/8/8/8/8/8/8/4K3 w - e3 0 1",b,o,&side,&ep,&castle);
    uint64_t phantom=generate_zobrist_key(b,side,ep,castle);
    position("4k3/8/8/8/8/8/8/4K3 w - - 0 1",b,o,&side,&ep,&castle);
    verdict("Phantom en-passant ignored in hash",
            phantom==generate_zobrist_key(b,side,ep,castle));
    // MATE VALIDATION REGRESSION: qsearch sees f3a3 mate in 3 at depth 1;
    // search must continue and prefer a4a8 mate in 2 at depth 3.
    position("8/1N2N3/2r5/3qp2R/QP2kp1K/5R2/6B1/6B1 w - - 0 1",
             b,o,&side,&ep,&castle);
    SearchOptions saved = search_get_options(), mate_opts = saved;
    mate_opts.max_time_ms = 5000;
    mate_opts.max_nodes = 1000000;
    mate_opts.verbose = 0;
    mate_opts.claim_draw = 0;
    search_set_options(mate_opts);
    clear_tt();
    Move fastest_mate = search_best_move(b,o,side,ep,castle,30);
    SearchStats mate_stats = search_get_stats();
    verdict("Prefer mate in 2 over qsearch mate in 3",
            MOVE_SRC(fastest_mate) == A4 && MOVE_TARGET(fastest_mate) == A8 &&
            mate_stats.completed_depth >= 3 && mate_stats.score == MATE_SCORE - 3 &&
            !mate_stats.stopped);
    verdict("PV begins with the completed best move",
            mate_stats.pv_count >= 1 && mate_stats.pv[0] == fastest_mate);
    search_set_options(saved);
    // Compare the qsearch generator with filtering all moves.
    const char *tactical_fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r6k/pp2r2p/4Rp1Q/3p4/8/1N1P2R1/PqP2bPP/7K b - - 0 24",
        "4k3/P6p/8/3pP3/8/8/p7/4K3 w - d6 0 1",
        "4k3/P6p/8/3pP3/8/8/p7/4K3 b - - 0 1"
    };
    int matching = 1;
    for (size_t f = 0; f < sizeof tactical_fens / sizeof tactical_fens[0]; f++) {
        position(tactical_fens[f],b,o,&side,&ep,&castle);
        MoveList all, tactical;
        generate_all_moves(&all,b,o,side,ep,castle);
        generate_tactical_moves(&tactical,b,o,side,ep);
        int count = 0;
        for (int i = 0; i < all.count; i++) {
            Move m = all.moves[i];
            if (!MOVE_IS_CAPTURE(m) && !MOVE_PROMOTED(m)) continue;
            count++;
            int found = 0;
            for (int j = 0; j < tactical.count; j++) found |= tactical.moves[j] == m;
            if (!found) { fprintf(stderr,"missing fen=%zu move=%u\n",f,m); matching = 0; }
        }
        if (count != tactical.count) { fprintf(stderr,"count fen=%zu full=%d tac=%d\n",f,count,tactical.count); matching = 0; }
    }
    verdict("Qsearch tactical generator matches all moves",matching);
}

static void benchmark_case(const char *name, const char *fen, int depth,
                           int time_limit_ms, uint64_t node_limit) {
    const char *labels[5] = {"baseline", "+aspiration", "+PVS", "+null move", "+LMR"};
    puts("\nposition            target mode          reached  nodes       seconds  score move   retry  null  LMR");
    for (int mode=0;mode<5;mode++) {
        Bitboard b[12],o[3];int side,ep,castle;
        position(fen,b,o,&side,&ep,&castle);
        clear_tt();
        SearchOptions opts={0};
        opts.aspiration=mode>=1;
        opts.pvs=mode>=2;
        opts.null_move=mode>=3;
        opts.lmr=mode>=4;
        opts.max_time_ms=time_limit_ms;
        opts.max_nodes=node_limit;
        // keep TT move ordering, avoid unverified score cutoffs.
        opts.tt_score_cutoffs=0;
        search_set_options(opts);
        clock_t start=clock();
        Move best=search_best_move(b,o,side,ep,castle,depth);
        SearchStats st=search_get_stats();
        double seconds=(double)(clock()-start)/CLOCKS_PER_SEC;
        char uci[7]="none";
        if(best) {
            uci[0]='a'+MOVE_SRC(best)%8;
            uci[1]='1'+MOVE_SRC(best)/8;
            uci[2]='a'+MOVE_TARGET(best)%8;
            uci[3]='1'+MOVE_TARGET(best)/8;
            uci[4]='\0';
        }
        printf("%-19s %2d     %-12s %2d%s      %-11" PRIu64 " %-7.3f  %5d %-6s %-5" PRIu64 " %-5" PRIu64 " %-5" PRIu64 "\n",
               name,depth,labels[mode],st.completed_depth,st.stopped?"*":" ",
               st.nodes,seconds,st.score,uci,st.aspiration_researches,
               st.null_cutoffs,st.lmr_researches);
        printf("  detail: q=%" PRIu64 " (%.1f%%) TT position=%" PRIu64 "/%" PRIu64
               " (%.1f%%) score=%" PRIu64 " NMP=%" PRIu64 "/%" PRIu64
               " LMR=%" PRIu64 "/%" PRIu64 " PVS retries=%" PRIu64 "\n",
               st.qnodes, st.nodes?100.0*st.qnodes/st.nodes:0.0,
               st.tt_key_hits, st.tt_probes,
               st.tt_probes?100.0*st.tt_key_hits/st.tt_probes:0.0,
               st.tt_score_hits, st.null_cutoffs, st.null_attempts,
               st.lmr_reductions, st.lmr_candidates, st.pvs_researches);
    }
}
// identical positions, cleared TT, and identical
// search settings except for the explicitly enabled feature in each row.
static void pruning_case(const char *name, const char *fen, int depth) {
    static const char *labels[] = {"off", "prefetch", "futility", "reverse", "razoring", "all"};
    puts("position mode       depth nodes       qnodes      seconds score move   fut-skip reverse razor-cut/try prefetch");
    for(int mode=0;mode<6;mode++) {
        Bitboard b[12],o[3],before[12],occ_before[3];int side,ep,castle;
        position(fen,b,o,&side,&ep,&castle);
        memcpy(before,b,sizeof(before));memcpy(occ_before,o,sizeof(occ_before));
        clear_tt();
        SearchOptions opts={0};
        opts.aspiration=opts.pvs=opts.null_move=opts.lmr=1;
        opts.tt_prefetch=(mode==1||mode==5);
        opts.futility=(mode==2||mode==5);
        opts.reverse_futility=(mode==3||mode==5);
        opts.razoring=(mode==4||mode==5);
        search_set_options(opts);
        clock_t start=clock();
        Move best=search_best_move(b,o,side,ep,castle,depth);
        SearchStats st=search_get_stats();
        double seconds=(double)(clock()-start)/CLOCKS_PER_SEC;
        char uci[6]="0000";
        if(best) {
            uci[0]='a'+MOVE_SRC(best)%8;uci[1]='1'+MOVE_SRC(best)/8;
            uci[2]='a'+MOVE_TARGET(best)%8;uci[3]='1'+MOVE_TARGET(best)/8;
            uci[4]=0;
        }
        verdict("Search restores board after selective pruning",
                !memcmp(before,b,sizeof(before)) && !memcmp(occ_before,o,sizeof(occ_before)));
        printf("%-8s %-10s %2d%s  %-11" PRIu64 " %-11" PRIu64 " %-7.3f %5d %-6s %-8" PRIu64 " %-7" PRIu64 " %-4" PRIu64 "/%-4" PRIu64 " %-8" PRIu64 "\n",
               name,labels[mode],st.completed_depth,st.stopped?"*":" ",st.nodes,st.qnodes,
               seconds,st.score,uci,st.futility_skips,st.reverse_futility_cutoffs,
               st.razor_cutoffs,st.razor_attempts,st.prefetches);
    }
}
static void pruning_terminal_checks(void) {
    // SELECTIVE PRUNING SAFETY: mates/stalemates and a quiet checking move
    // must survive reverse futility, razoring, and quiet-move futility.
    static const char *fens[] = {
        "7k/6Q1/5K2/8/8/8/8/8 b - - 0 1", // checkmate
        "7k/5K2/6Q1/8/8/8/8/8 b - - 0 1", // stalemate
        "7k/8/5KQ1/8/8/8/8/8 w - - 0 1", // quiet mate available
        "8/8/8/3k4/8/8/4P3/4K3 w - - 0 1" // pawn-only zugzwang risk
    };
    for(size_t i=0;i<sizeof(fens)/sizeof(fens[0]);i++) {
        Move baseline=0;
        int baseline_score=0;
        for(int mode=0;mode<2;mode++) {
            Bitboard b[12],o[3];int side,ep,castle;
            position(fens[i],b,o,&side,&ep,&castle);
            clear_tt();
            SearchOptions opts={0};
            opts.aspiration=opts.pvs=opts.null_move=opts.lmr=1;
            opts.futility=opts.reverse_futility=opts.razoring=mode;
            opts.tt_prefetch=mode;
            search_set_options(opts);
            Move best=search_best_move(b,o,side,ep,castle,5);
            SearchStats st=search_get_stats();
            if(!mode) {baseline=best;baseline_score=st.score;}
            else {
                char label[80];
                snprintf(label,sizeof(label),"Pruning terminal/quiet safety case %zu",i+1);
                verdict(label,best==baseline && st.score==baseline_score);
            }
        }
    }
}
// SINGULAR DIAGNOSTICS: identical limits, positions and TT reset per run.
static void singular_case(const char *name, const char *fen, int depth, int ms) {
    for (int on = 0; on <= 1; on++) {
        Bitboard b[12], o[3], original[12], occ_original[3];
        int side, ep, castle;
        position(fen, b, o, &side, &ep, &castle);
        memcpy(original, b, sizeof(original));
        memcpy(occ_original, o, sizeof(occ_original));
        clear_tt();
        SearchOptions opts = {0};
        opts.aspiration = opts.pvs = opts.null_move = opts.lmr = 1;
        opts.futility = opts.reverse_futility = opts.razoring = opts.tt_prefetch = 1;
        opts.singular = on;
        opts.max_time_ms = ms;
        opts.max_nodes = 8000000;
        search_set_options(opts);
        clock_t start = clock();
        Move m = search_best_move(b, o, side, ep, castle, depth);
        SearchStats st = search_get_stats();
        char uci[6] = "0000";
        if (m) {
            uci[0] = 'a' + MOVE_SRC(m) % 8;
            uci[1] = '1' + MOVE_SRC(m) / 8;
            uci[2] = 'a' + MOVE_TARGET(m) % 8;
            uci[3] = '1' + MOVE_TARGET(m) / 8;
            uci[4] = 0;
        }
        verdict("Singular search restores board and occupancy",
                !memcmp(original, b, sizeof(original)) &&
                !memcmp(occ_original, o, sizeof(occ_original)));
        printf("singular %-8s %s reached=%d/%d%s nodes=%" PRIu64
               " q=%" PRIu64 " cpu_s=%.3f score=%d move=%s "
               "attempted=%" PRIu64 " extended=%" PRIu64 " refuted=%" PRIu64 "\n",
               name, on ? "on " : "off", st.completed_depth, depth,
               st.stopped ? "*" : "", st.nodes, st.qnodes,
               (double)(clock() - start) / CLOCKS_PER_SEC, st.score, uci,
               st.singular_attempts, st.singular_extensions, st.singular_refutations);
    }
}
static void usage(const char *exe) {
    fprintf(stderr, "Usage: %s [--quick|--stress] [--pruning|--singular] "
            "[--benchmark-only] [--depth N]\n", exe);
}
int main(int argc,char **argv) {
    int stress=0, one_depth=0, pruning=0, singular=0, benchmark_only=0;
    for(int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--stress")) stress=1;
        else if (!strcmp(argv[i],"--quick")) stress=0;
        else if (!strcmp(argv[i],"--pruning")) pruning=1;
        else if (!strcmp(argv[i],"--singular")) singular=1;
        else if (!strcmp(argv[i],"--benchmark-only")) benchmark_only=1;
        else if (!strcmp(argv[i],"--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i],"--depth") && i+1<argc) {
            char *end=NULL;
            long value=strtol(argv[++i],&end,10);
            if (!*argv[i] || *end || value<1 || value>=MAX_PLY) {
                usage(argv[0]);return 2;
            }
            one_depth=(int)value;
        } else { usage(argv[0]);return 2; }
    }
    if (pruning && singular) { usage(argv[0]);return 2; }
    init_sliders_attacks();init_leaper_attacks();init_eval_tables();init_tt(32);
    if (!benchmark_only) correctness_suite();
    if (failures) { fprintf(stderr,"%d correctness check(s) failed; benchmarks skipped.\n",failures);free_tt();return 1; }
    puts("\nBENCHMARK: * indicates stopped by the per-run time/node limit; compare only completed depths.");
    const char *opening="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const char *tactic="r6k/pp2r2p/4Rp1Q/3p4/8/1N1P2R1/PqP2bPP/7K b - - 0 24";
    if(singular) {
        int depth=one_depth?one_depth:15;
        if(depth<1 || depth>=MAX_PLY) {free_tt();return 2;}
        singular_case("opening", opening, depth, stress?3500:2500);
        singular_case("tactic", tactic, depth, stress?3500:2500);
    } else if(pruning) {
        int depth=one_depth?one_depth:8;
        if(depth<1 || depth>=MAX_PLY) { free_tt();return 2; }
        pruning_terminal_checks();
        pruning_case("opening",opening,depth);
        pruning_case("tactic",tactic,depth);
    } else if(one_depth) {
        if(one_depth<1 || one_depth>=MAX_PLY) { free_tt();return 2; }
        benchmark_case("opening",opening,one_depth,stress?3500:2500,stress?8000000:5000000);
        benchmark_case("tactic",tactic,one_depth,stress?3500:2500,stress?8000000:5000000);
    } else {
        int depths[]={5,7,9,15};
        int count=stress?4:3;
        for(int i=0;i<count;i++) {
            int ms=stress?3500:2500;
            uint64_t nodes=stress?8000000:5000000;
            benchmark_case("opening",opening,depths[i],ms,nodes);
            benchmark_case("tactic",tactic,depths[i],ms,nodes);
        }
    }
    free_tt();
    if(failures) { fprintf(stderr,"%d correctness check(s) failed.\n",failures); return 1; }
    puts("\nAll correctness checks passed.");
    return 0;
}
