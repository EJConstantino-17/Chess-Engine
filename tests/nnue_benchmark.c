/* 9/30/2026 12:05: Compare full refresh with incremental features, then PeSTO/NNUE searches. */
#include "../inc/eval.h"
#include "../inc/movegen.h"
#include "../inc/search.h"
#include "../inc/tt.h"
#include "../inc/perft.h"
#include "../nnue/cce_nnue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef CCE_NNUE
#error Build this diagnostic with NNUE=1
#endif
static int failures, checks, exported;
static unsigned rng=20260930;
static unsigned random32(void) { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static const char *positions[]={
 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
 "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
 "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
 "4r1k1/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
 "4k3/P7/8/8/8/8/7p/4K3 w - - 0 1",
 "4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1",
 "8/3P3k/n2K3p/2p3n1/1b4N1/2p1p1P1/8/3B4 w - - 0 1",
 "r6k/pp2r2p/4Rp1Q/3p4/8/1N1P2R1/PqP2bPP/7K b - - 0 24"
};
static void require(int ok,const char *name) { if(!ok) { fprintf(stderr,"FAIL: %s\n",name); ++failures; } }
static void validate(Bitboard b[12],int side) {
    ++checks;
    require(cce_nnue_validate(b,side),"incremental accumulator/PSQT/raw output equals cold refresh");
}
static void dump(Bitboard b[12],int side,int ep,int castle) {
    if(is_square_attacked(get_lsb_index(b[side==WHITE?K:k]),b,
        (Bitboard[3]){b[P]|b[N]|b[B]|b[R]|b[Q]|b[K],b[p]|b[n]|b[8]|b[r]|b[q]|b[k],
        b[P]|b[N]|b[B]|b[R]|b[Q]|b[K]|b[p]|b[n]|b[8]|b[r]|b[q]|b[k]},side^1)) return;
    printf("%d\t",cce_nnue_raw(b,side,0));
    const char *letters="PNBRQKpnbrqk";
    for(int rank=7;rank>=0;--rank) {
        int empty=0;
        for(int file=0;file<8;++file) {
            int found=-1;
            for(int pt=0;pt<12;++pt) if(b[pt]&(UINT64_C(1)<<(rank*8+file))) { found=pt;break; }
            if(found<0) ++empty;
            else { if(empty) printf("%d",empty);empty=0;putchar(letters[found]); }
        }
        if(empty) printf("%d",empty);
        if(rank) putchar('/');
    }
    printf(" %c ",side==WHITE?'w':'b');
    if(!castle) putchar('-');
    else { if(castle&1) putchar('K');if(castle&2) putchar('Q');if(castle&4) putchar('k');if(castle&8) putchar('q'); }
    if(ep<0) printf(" - 0 1\n");else printf(" %c%c 0 1\n",'a'+ep%8,'1'+ep/8);
    ++exported;
}
static void walk(Bitboard b[12],Bitboard o[3],int side,int ep,int castle,int ply,int remaining,int *budget,int oracle) {
    if(*budget<=0 || failures) return;
    --*budget;validate(b,side);
    if(oracle) dump(b,side,ep,castle);
    if(!remaining) return;
    int before=cce_nnue_raw(b,side,0);
    MoveList list;generate_all_moves(&list,b,o,side,ep,castle);
    for(int i=0;i<list.count && *budget>0;++i) {
        int s=side,e=ep,c=castle;
        if(make_move(list.moves[i],b,o,&s,&e,&c,ply)) {
            walk(b,o,s,e,c,ply+1,remaining-1,budget,oracle);
            unmake_move(list.moves[i],b,o,&s,&e,&c,ply);
        }
        require(cce_nnue_raw(b,side,0)==before,"undo/rejected move restores evaluation");
    }
}
static void random_game(int oracle) {
    Bitboard b[12],o[3];int side,ep,castle;
    parse_fen(positions[0],b,o,&side,&ep,&castle);cce_nnue_reset();
    Move path[100];int made=0;
    for(int ply=0;ply<100;++ply) {
        validate(b,side); if(oracle && ply%4==0) dump(b,side,ep,castle);
        MoveList list;generate_all_moves(&list,b,o,side,ep,castle);
        if(!list.count) break;
        int start=random32()%(unsigned)list.count,legal=0;
        for(int j=0;j<list.count;++j) {
            Move m=list.moves[(start+j)%list.count];
            if(make_move(m,b,o,&side,&ep,&castle,ply)) { path[made++]=m;legal=1;break; }
        }
        if(!legal) break;
    }
    while(made) { --made;unmake_move(path[made],b,o,&side,&ep,&castle,made);validate(b,side); }
}
static void correctness(int oracle) {
    for(unsigned i=0;i<sizeof positions/sizeof *positions;++i) {
        Bitboard b[12],o[3];int side,ep,castle;
        parse_fen(positions[i],b,o,&side,&ep,&castle);cce_nnue_reset();
        int budget=250;walk(b,o,side,ep,castle,0,3,&budget,oracle);
    }
    for(int i=0;i<12;++i) random_game(oracle);
    Bitboard b[12],o[3];int side,ep,castle;
    parse_fen(positions[0],b,o,&side,&ep,&castle);cce_nnue_reset();
    int before=cce_nnue_raw(b,side,0);
    cce_nnue_null(b,side,ep,castle);validate(b,side^1);
    int budget=80;walk(b,o,side^1,-1,castle,1,2,&budget,oracle);
    cce_nnue_undo_null();require(cce_nnue_raw(b,side,0)==before,"null move undo");
    // 9/30/2026 12:05: CCE permits deeper paths than the upstream accumulator stack.
    parse_fen(positions[0],b,o,&side,&ep,&castle);cce_nnue_reset();
    Move deep_path[252];
    const int from[4]={G1,G8,F3,F6},to[4]={F3,F6,G1,G8};
    int deep_count=0;
    for(int ply=0;ply<252;++ply) {
        MoveList list;generate_all_moves(&list,b,o,side,ep,castle);Move chosen=0;
        for(int j=0;j<list.count;++j) if(MOVE_SRC(list.moves[j])==(unsigned)from[ply%4]
            && MOVE_TARGET(list.moves[j])==(unsigned)to[ply%4]) {chosen=list.moves[j];break;}
        if(!chosen || !make_move(chosen,b,o,&side,&ep,&castle,ply)) { require(0,"deep path legal move");break; }
        deep_path[deep_count++]=chosen;validate(b,side);
    }
    while(deep_count) { --deep_count;unmake_move(deep_path[deep_count],b,o,&side,&ep,&castle,deep_count);validate(b,side); }
    require(cce_nnue_updates()>2000,"incremental moves were exercised");
    if(!oracle) printf("Correctness: %d full accumulator/PSQT/output comparisons, updates=%" PRIu64 ", failures=%d\n",checks,cce_nnue_updates(),failures);
}
static void bench(int fixed_ms) {
    puts("mode,backend,case,depth,score_cp,bestmove,nodes,qnodes,cpu_seconds,nps");
    unsigned cases[]={0,1,7,8};
    for(unsigned j=0;j<4;++j) for(int backend=0;backend<2;++backend) {
        unsigned i=cases[j];Bitboard b[12],o[3],original[12];int side,ep,castle;
        parse_fen(positions[i],b,o,&side,&ep,&castle);memcpy(original,b,sizeof b);
        cce_nnue_enable(backend);clear_tt();
        SearchOptions opt=search_get_options();opt.verbose=0;opt.max_time_ms=fixed_ms;opt.max_nodes=0;
        search_set_options(opt);
        clock_t begin=clock();Move m=search_best_move(b,o,side,ep,castle,fixed_ms?30:6);
        double seconds=(double)(clock()-begin)/CLOCKS_PER_SEC;
        SearchStats st=search_get_stats();
        char text[6]="0000";
        if(m) { text[0]='a'+MOVE_SRC(m)%8;text[1]='1'+MOVE_SRC(m)/8;text[2]='a'+MOVE_TARGET(m)%8;text[3]='1'+MOVE_TARGET(m)/8;
            if(MOVE_PROMOTED(m)) { text[4]="pnbrqk"[MOVE_PROMOTED(m)%6];text[5]=0; } }
        printf("%s,%s,%u,%d,%d,%s,%" PRIu64 ",%" PRIu64 ",%.6f,%.0f\n",fixed_ms?"fixed_time":"fixed_depth",backend?"NNUE":"PeSTO",i,st.completed_depth,st.score,text,st.nodes,st.qnodes,seconds,seconds>0?st.nodes/seconds:0);
        require(!memcmp(b,original,sizeof b),"benchmark restores CCE board");
        if(backend) validate(b,side);
    }
    cce_nnue_enable(1);
}
int main(int argc,char **argv) {
    if(argc<2) { fprintf(stderr,"Usage: %s network.nnue [--check|--oracle|--bench]\n",argv[0]);return 2; }
    init_sliders_attacks();init_leaper_attacks();init_eval_tables();init_zobrist();init_tt(32);
    if(!cce_nnue_load(argv[1])) { fprintf(stderr,"Load failed: %s\n",cce_nnue_error());return 1; }
    if(argc>3 && !strcmp(argv[2],"--reject")) {
        Bitboard b[12],o[3];int side,ep,castle;
        parse_fen(positions[0],b,o,&side,&ep,&castle);
        int before=cce_nnue_raw(b,side,0);
        require(!cce_nnue_load(argv[3]),"damaged network rejected");
        require(cce_nnue_enabled() && cce_nnue_raw(b,side,0)==before,"rejection preserves loaded network");
        printf("Rejected damaged file; prior network preserved: %s\n",failures?"FAIL":"PASS");
        free_tt();return failures?1:0;
    }
    int oracle=argc>2 && !strcmp(argv[2],"--oracle");
    if(!(argc>2 && !strcmp(argv[2],"--bench"))) correctness(oracle);
    if(!failures && !oracle && !(argc>2 && !strcmp(argv[2],"--check"))) { bench(0);bench(1500); }
    if(!oracle) printf("Total failures: %d\n",failures);
    free_tt();return failures?1:0;
}
