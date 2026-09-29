// text protocol shared by UCI GUIs. Keep stdout free of game-loop text.
#include "uci.h"
#include "search.h"
#include "game_loop.h"
#include "tt.h"
#include "opening_book.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <conio.h> // MinGW declares _kbhit here, not in io.h.
#else
#include <unistd.h>
#include <sys/select.h>
#endif

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
typedef struct {
    Bitboard pieces[12], occ[3];
    int side, ep, castle;
    SearchState history;
    char partial[8192], pending[8192];
    size_t length;
    int has_pending, stop, quit;
    int own_book, move_time_cap_ms;
    char book_file[512];
    char book_index[512];
} Uci;

static void reset_position(Uci *u, const char *fen) {
    parse_fen(fen,u->pieces,u->occ,&u->side,&u->ep,&u->castle);
    int halfmove=0;
    if(sscanf(fen,"%*s %*s %*s %*s %d",&halfmove)!=1) halfmove=0;
    search_state_reset(&u->history,u->pieces,u->side,u->ep,u->castle,halfmove);
}
static int input_ready(void) {
#ifdef _WIN32
    HANDLE h=GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, available=0;
    if(GetConsoleMode(h,&mode)) return _kbhit()!=0;
    return PeekNamedPipe(h,NULL,0,NULL,&available,NULL) && available>0;
#else
    fd_set fds; struct timeval tv={0,0};
    FD_ZERO(&fds); FD_SET(STDIN_FILENO,&fds);
    return select(STDIN_FILENO+1,&fds,NULL,NULL,&tv)>0;
#endif
}
static int read_byte(char *out) {
#ifdef _WIN32
    DWORD got=0;
    return ReadFile(GetStdHandle(STD_INPUT_HANDLE),out,1,&got,NULL) && got==1;
#else
    return read(STDIN_FILENO,out,1)==1;
#endif
}
// poll during search so 'stop', 'quit', and 'isready' are handled promptly.
static int poll_input(Uci *u, int blocking) {
    while(blocking || input_ready()) {
        char c;
        if(!read_byte(&c)) {u->quit=1;u->stop=1;return 0;}
        if(c=='\r') continue;
        if(c!='\n') {
            if(u->length+1<sizeof(u->partial)) u->partial[u->length++]=c;
            continue;
        }
        u->partial[u->length]=0;
        u->length=0;
        if(blocking) return 1;
        if(!strcmp(u->partial,"stop")) u->stop=1;
        else if(!strcmp(u->partial,"quit")) {u->quit=1;u->stop=1;}
        else if(!strcmp(u->partial,"isready")) {puts("readyok");fflush(stdout);}
        else if(!u->has_pending) {
            strcpy(u->pending,u->partial);u->has_pending=1;u->stop=1;
        }
        if(u->stop) return 1;
    }
    return 1;
}
static int stop_hook(void *ctx) {
    Uci *u=ctx;
    poll_input(u,0);
    return u->stop;
}
static void format_move(Move m,char out[6]) {
    if(!m) {strcpy(out,"0000");return;}
    out[0]='a'+MOVE_SRC(m)%8;out[1]='1'+MOVE_SRC(m)/8;
    out[2]='a'+MOVE_TARGET(m)%8;out[3]='1'+MOVE_TARGET(m)/8;
    out[4]=0;
    int p=MOVE_PROMOTED(m);
    if(p) {out[4]="pnbrqk"[p%6];out[5]=0;}
}
static int apply_move(Uci *u,const char *text) {
    Move m;
    if(!uci_to_move(text,u->pieces,u->occ,u->side,u->ep,u->castle,&m)) return 0;
    if(!make_move(m,u->pieces,u->occ,&u->side,&u->ep,&u->castle,0)) return 0;
    search_state_record_move(&u->history,m,u->pieces,u->side,u->ep,u->castle);
    return 1;
}
static void set_position(Uci *u,char *line) {
    char *p=line+8;
    while(*p==' ')p++;
    if(!strncmp(p,"startpos",8) && (!p[8] || p[8]==' ')) {
        reset_position(u,START_FEN);p+=8;
    } else if(!strncmp(p,"fen ",4)) {
        p+=4;
        char *moves=strstr(p," moves ");
        if(moves) *moves=0;
        // Reject obviously malformed FEN before the legacy parser touches it.
        char board[96],stm[8],rights[8],ep[8];
        if(sscanf(p,"%95s %7s %7s %7s",board,stm,rights,ep)!=4 ||
           !strchr(board,'/') || (strcmp(stm,"w") && strcmp(stm,"b"))) return;
        reset_position(u,p);
        p=moves?moves+1:p+strlen(p);
    } else return;
    while(*p==' ')p++;
    if(strncmp(p,"moves",5) || (p[5] && p[5]!=' ')) return;
    p+=5;
    while(*p) {
        while(*p==' ')p++;
        if(!*p)break;
        char *end=p;
        while(*end && *end!=' ')end++;
        char saved=*end;*end=0;
        int valid=apply_move(u,p);
        *end=saved;
        if(!valid)break;
        p=end;
    }
}
static int get_number(const char *line,const char *name,int fallback) {
    char copy[512];snprintf(copy,sizeof(copy),"%s",line);
    char *token=strtok(copy," \t");
    while(token) {
        if(!strcmp(token,name)) {
            token=strtok(NULL," \t");
            if(token) {char *end;long n=strtol(token,&end,10);
                if(*end==0 && n>=0 && n<1000000000L)return (int)n;}
            return fallback;
        }
        token=strtok(NULL," \t");
    }
    return fallback;
}
static void go(Uci *u,const char *line) {
    SearchOptions saved=search_get_options(), opts=saved;
    int depth=get_number(line,"depth",MAX_PLY-2);
    int movetime=get_number(line,"movetime",-1);
    int clock_ms=get_number(line,u->side==WHITE?"wtime":"btime",-1);
    int inc=get_number(line,u->side==WHITE?"winc":"binc",0);
    // play instantly in a timed game; explicit depth and
    // infinite-analysis requests still run the search for useful analysis.
    if (u->own_book && (movetime >= 0 || clock_ms >= 0)) {
        int index_loaded=0;
        Move book=opening_book_pick_index(u->book_index,u->pieces,u->occ,
                                           u->side,u->ep,u->castle,&index_loaded);
        // a valid index decides in logarithmic time; do not
        // rescan a potentially huge text dataset after an indexed miss.
        if(!index_loaded)
            book=opening_book_pick(u->book_file,&u->history,u->pieces,u->occ,
                                   u->side,u->ep,u->castle);
        if (book) {
            char text[6];format_move(book,text);
            printf("info string book move %s\nbestmove %s\n",text,text);
            fflush(stdout);
            return;
        }
    }
    int moves_to_go=get_number(line,"movestogo",30);
    if(moves_to_go<1)moves_to_go=30;
    if(depth<1)depth=1;
    if(depth>=MAX_PLY)depth=MAX_PLY-2;
    opts.max_time_ms=0;opts.max_nodes=0;opts.verbose=0;opts.claim_draw=0;
    if(movetime>=0) opts.max_time_ms=movetime;
    else if(clock_ms>=0) {
        int budget=clock_ms/moves_to_go+inc/2;
        int reserve=clock_ms/20+10;
        if(budget>clock_ms-reserve)budget=clock_ms-reserve;
        opts.max_time_ms=budget>1?budget:1;
        if (u->move_time_cap_ms > 0 && opts.max_time_ms > u->move_time_cap_ms)
            opts.max_time_ms=u->move_time_cap_ms;
    }
    int nodes=get_number(line,"nodes",0);
    if(nodes>0)opts.max_nodes=(uint64_t)nodes;
    u->stop=0;
    search_set_options(opts);
    search_set_stop_hook(stop_hook,u);
    Move best=search_best_move_with_state(u->pieces,u->occ,u->side,u->ep,u->castle,depth,&u->history);
    search_set_stop_hook(NULL,NULL);
    SearchStats st=search_get_stats();
    search_set_options(saved);
    char move[6];format_move(best,move);
    printf("info depth %d score cp %d nodes %llu\n",st.completed_depth,st.score,(unsigned long long)st.nodes);
    printf("bestmove %s\n",move);fflush(stdout);
}
int uci_loop(void) {
    Uci u={0};reset_position(&u,START_FEN);
    u.own_book=1;
    u.move_time_cap_ms=2000;
    strcpy(u.book_file,"opening_book.txt");
    strcpy(u.book_index,"opening_book.cbk");
    for(;;) {
        if(u.has_pending) {
            strcpy(u.partial,u.pending);u.has_pending=0;
        } else if(!poll_input(&u,1))break;
        if(!strcmp(u.partial,"uci")) {
            puts("id name CCE Chess Engine");puts("id author Engine Project");
            puts("option name Hash type spin default 64 min 1 max 1024");
            puts("option name Clear Hash type button");
            puts("option name OwnBook type check default true");
            puts("option name BookFile type string default opening_book.txt");
            puts("option name BookIndex type string default opening_book.cbk");
            puts("option name Move Time Cap type spin default 2000 min 0 max 60000");
            puts("uciok");fflush(stdout);
        } else if(!strcmp(u.partial,"isready")) {puts("readyok");fflush(stdout);}
        else if(!strcmp(u.partial,"ucinewgame")) {clear_tt();reset_position(&u,START_FEN);}
        else if(!strncmp(u.partial,"position ",9)) set_position(&u,u.partial);
        else if(!strncmp(u.partial,"setoption name Hash value ",26)) {
            int mb=atoi(u.partial+26);if(mb>=1&&mb<=1024)init_tt((size_t)mb);
        } else if(!strcmp(u.partial,"setoption name Clear Hash"))clear_tt();
        else if(!strncmp(u.partial,"setoption name OwnBook value ",29))
            u.own_book=strcmp(u.partial+29,"false") && strcmp(u.partial+29,"0");
        else if(!strncmp(u.partial,"setoption name BookFile value ",30)) {
            size_t len=strlen(u.partial+30);
            if (len && len<sizeof(u.book_file)) memcpy(u.book_file,u.partial+30,len+1);
        } else if(!strncmp(u.partial,"setoption name BookIndex value ",31)) {
            size_t len=strlen(u.partial+31);
            if (len && len<sizeof(u.book_index)) memcpy(u.book_index,u.partial+31,len+1);
        } else if(!strncmp(u.partial,"setoption name Move Time Cap value ",35)) {
            int cap=atoi(u.partial+35);
            if(cap>=0 && cap<=60000)u.move_time_cap_ms=cap;
        }
        else if(!strncmp(u.partial,"go",2) && (!u.partial[2]||u.partial[2]==' '))go(&u,u.partial);
        else if(!strcmp(u.partial,"quit"))break;
        if(u.quit)break;
    }
    return 0;
}
