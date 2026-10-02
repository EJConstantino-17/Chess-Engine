#include "../inc/search.h"
#include "../inc/eval.h"
#include "../inc/tt.h"
#include "../nnue/cce_nnue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static const char *fens[] = {
"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
"r1bqk1nr/pppp1pp1/2nb3p/4p3/3PP3/2P2N2/PP3PPP/RNBQKB1R w KQkq - 0 1",
"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
"8/3P3k/n2K3p/2p3n1/1b4N1/2p1p1P1/8/3B4 w - - 0 1",
"r6k/pp2r2p/4Rp1Q/3p4/8/1N1P2R1/PqP2bPP/7K b - - 0 24",
"8/8/8/3k4/8/8/4P3/4K3 w - - 0 1"};
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr,"Usage: %s network.nnue [depth [milliseconds [--tt-history]]]\n",argv[0]); return 2; }
    int depth = argc > 2 ? atoi(argv[2]) : 6, ms = argc > 3 ? atoi(argv[3]) : 0;
    if (depth < 1 || depth >= MAX_PLY || ms < 0) return 2;
    init_sliders_attacks(); init_leaper_attacks(); init_eval_tables(); init_zobrist(); init_tt(32);
    if (!cce_nnue_load(argv[1])) { fprintf(stderr,"NNUE load failed: %s\n",cce_nnue_error()); return 2; }
    int history_suite = argc > 4 && !strcmp(argv[4], "--tt-history");
    const char *history_names[] = {"current", "tt_only", "history_only", "history_lmr", "combined"};
    const char *names[] = {"baseline","delta","see","lmr","rfp","combined","tt","combined_tt"};
    printf("mode,case,target_depth,budget_ms,depth,score,move,nodes,qnodes,ms,delta_skips,see_skips,lmr_reductions,rfp_cutoffs,tt_hits,history_bonuses,history_maluses,history_lmr_less,history_lmr_more,pv_count\n");
    for (int mode = 0; mode < (history_suite ? 5 : 8); ++mode) for (int position = 0; position < 6; ++position) {
        Bitboard b[12], o[3], saved[12], saved_occ[3]; int side, ep, castle;
        parse_fen(fens[position],b,o,&side,&ep,&castle); memcpy(saved,b,sizeof b); memcpy(saved_occ,o,sizeof o);
        clear_tt(); cce_nnue_reset();
        SearchOptions opt = search_get_options();
        opt.q_delta = mode == 1 || mode == 5 || mode == 7;
        opt.q_see = mode == 2 || mode == 5 || mode == 7;
        opt.dynamic_lmr = mode == 3 || mode == 5 || mode == 7;
        opt.tuned_rfp = mode == 4 || mode == 5 || mode == 7;
        opt.tt_score_cutoffs = mode >= 6;
        opt.history_tuning = opt.history_lmr = 0;
        if (history_suite) {
            opt.q_delta = opt.q_see = opt.dynamic_lmr = opt.tuned_rfp = 1;
            opt.tt_score_cutoffs = mode == 1 || mode == 4;
            opt.history_tuning = mode >= 2;
            opt.history_lmr = mode >= 3;
        }
        opt.verbose = 0; opt.claim_draw = 0;
        opt.max_time_ms = ms ? ms : (history_suite ? 15000 : 5000);
        opt.max_nodes = history_suite ? 10000000 : 3000000;
        search_set_options(opt);
        Move m = search_best_move(b,o,side,ep,castle,depth); SearchStats st = search_get_stats();
        char move_text[6] = "0000";
        if (m) {
            move_text[0]='a'+MOVE_SRC(m)%8; move_text[1]='1'+MOVE_SRC(m)/8;
            move_text[2]='a'+MOVE_TARGET(m)%8; move_text[3]='1'+MOVE_TARGET(m)/8;
            if (MOVE_PROMOTED(m)) { move_text[4]="pnbrqk"[MOVE_PROMOTED(m)%6]; move_text[5]=0; }
        }
        printf("%s,%d,%d,%d,%d,%d,%s,%" PRIu64 ",%" PRIu64 ",%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d\n",
            history_suite ? history_names[mode] : names[mode],position,depth,ms,st.completed_depth,st.score,move_text,
            st.nodes,st.qnodes,st.elapsed_ms,st.q_delta_skips,st.q_see_skips,st.lmr_reductions,st.reverse_futility_cutoffs,st.tt_score_hits,st.history_bonuses,st.history_maluses,
            st.history_lmr_less,st.history_lmr_more,st.pv_count);
        if (memcmp(b,saved,sizeof b) || memcmp(o,saved_occ,sizeof o) || !cce_nnue_validate(b,side)) return 1;
        if (st.pv_count && st.pv[0] != m) return 1;
        int s=side,e=ep,c=castle,made=0;
        for (int j=0;j<st.pv_count;++j) {
            if (!make_move(st.pv[j],b,o,&s,&e,&c,j)) return 1;
            ++made;
        }
        while (made) { --made; unmake_move(st.pv[made],b,o,&s,&e,&c,made); }
        if (memcmp(b,saved,sizeof b) || memcmp(o,saved_occ,sizeof o) || !cce_nnue_validate(b,side)) return 1;
    }
    free_tt(); return 0;
}
