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
    if (argc < 2) { fprintf(stderr,"Usage: %s network.nnue [depth [milliseconds]]\n",argv[0]); return 2; }
    int depth = argc > 2 ? atoi(argv[2]) : 6, ms = argc > 3 ? atoi(argv[3]) : 0;
    if (depth < 1 || depth >= MAX_PLY || ms < 0) return 2;
    init_sliders_attacks(); init_leaper_attacks(); init_eval_tables(); init_zobrist(); init_tt(32);
    if (!cce_nnue_load(argv[1])) { fprintf(stderr,"NNUE load failed: %s\n",cce_nnue_error()); return 2; }
    const char *names[] = {"baseline","delta","see","lmr","rfp","combined","tt","combined_tt"};
    printf("mode,case,target_depth,budget_ms,depth,score,move,nodes,qnodes,ms,delta_skips,see_skips,lmr_reductions,rfp_cutoffs,tt_hits\n");
    for (int mode = 0; mode < 8; ++mode) for (int position = 0; position < 6; ++position) {
        Bitboard b[12], o[3], saved[12]; int side, ep, castle;
        parse_fen(fens[position],b,o,&side,&ep,&castle); memcpy(saved,b,sizeof b);
        clear_tt(); cce_nnue_reset();
        SearchOptions opt = search_get_options();
        opt.q_delta = mode == 1 || mode == 5 || mode == 7;
        opt.q_see = mode == 2 || mode == 5 || mode == 7;
        opt.dynamic_lmr = mode == 3 || mode == 5 || mode == 7;
        opt.tuned_rfp = mode == 4 || mode == 5 || mode == 7;
        opt.tt_score_cutoffs = mode >= 6; opt.verbose = 0; opt.claim_draw = 0;
        opt.max_time_ms = ms ? ms : 5000; opt.max_nodes = 3000000;
        search_set_options(opt);
        Move m = search_best_move(b,o,side,ep,castle,depth); SearchStats st = search_get_stats();
        char move_text[6] = "0000";
        if (m) {
            move_text[0]='a'+MOVE_SRC(m)%8; move_text[1]='1'+MOVE_SRC(m)/8;
            move_text[2]='a'+MOVE_TARGET(m)%8; move_text[3]='1'+MOVE_TARGET(m)/8;
            if (MOVE_PROMOTED(m)) { move_text[4]="pnbrqk"[MOVE_PROMOTED(m)%6]; move_text[5]=0; }
        }
        printf("%s,%d,%d,%d,%d,%d,%s,%" PRIu64 ",%" PRIu64 ",%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
            names[mode],position,depth,ms,st.completed_depth,st.score,move_text,
            st.nodes,st.qnodes,st.elapsed_ms,st.q_delta_skips,st.q_see_skips,st.lmr_reductions,st.reverse_futility_cutoffs,st.tt_score_hits);
        if (memcmp(b,saved,sizeof b) || !cce_nnue_validate(b,side)) return 1;
    }
    free_tt(); return 0;
}
