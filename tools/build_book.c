// offline compiler, standalone compiler for UCI move lines.
// Usage: build_book OUTPUT.cbk INPUT.txt [MORE.txt ...]
// Inputs are one UCI line each, or TSV with a named 'uci' column.
#include "book_index.h"
#include "tt.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct { BookRecord *v; size_t n, cap; } Records;
static int append(Records *r, BookRecord record) {
    if(r->n==r->cap) {
        if(r->cap>SIZE_MAX/2/sizeof(BookRecord)) return 0;
        size_t next=r->cap?r->cap*2:4096;
        BookRecord *v=realloc(r->v,next*sizeof(BookRecord));
        if(!v)return 0;
        r->v=v;r->cap=next;
    }
    r->v[r->n++]=record;
    return 1;
}
static int compare_record(const void *a,const void *b) {
    const BookRecord *x=a,*y=b;
    if(x->key!=y->key)return x->key<y->key?-1:1;
    if(x->move!=y->move)return x->move<y->move?-1:1;
    return 0;
}
static int uci_move(char *text,Bitboard p[12],Bitboard o[3],int side,int ep,int castle,Move *out) {
    size_t len=strlen(text);
    if((len!=4 && len!=5)||text[0]<'a'||text[0]>'h'||text[2]<'a'||text[2]>'h'||
       text[1]<'1'||text[1]>'8'||text[3]<'1'||text[3]>'8')return 0;
    int from=(text[0]-'a')+8*(text[1]-'1');
    int to=(text[2]-'a')+8*(text[3]-'1');
    MoveList list;generate_all_moves(&list,p,o,side,ep,castle);
    for(int i=0;i<list.count;i++) {
        Move m=list.moves[i];
        if(MOVE_SRC(m)!=(unsigned)from||MOVE_TARGET(m)!=(unsigned)to)continue;
        int promo=MOVE_PROMOTED(m);
        if((len==4 && promo)||(len==5 && (!promo ||
             "pnbrqk"[promo%6]!=text[4])))continue;
        *out=m;return 1;
    }
    return 0;
}
static int process_line(char *line,Records *all) {
    Bitboard p[12],o[3];init_board(p,o);
    int side=WHITE,ep=NO_SQUARE,castle=15;
    BookRecord partial[24];int n=0;
    for(char *tok=strtok(line," \t\r\n");tok && n<24;
        tok=strtok(NULL," \t\r\n")) {
        Move m=0;
        if(!uci_move(tok,p,o,side,ep,castle,&m))return 0;
        BookRecord r={generate_zobrist_key(p,side,ep,castle),m,1};
        if(!make_move(m,p,o,&side,&ep,&castle,0))return 0;
        partial[n++]=r;
    }
    if(!n)return 0;
    for(int i=0;i<n;i++)if(!append(all,partial[i])) {
        fprintf(stderr,"Out of memory building book\n");exit(1);
    }
    return 1;
}
static int tsv_uci_column(const char *header) {
    char line[8192];snprintf(line,sizeof(line),"%s",header);
    int column=0;
    char *start=line;
    while(start) {
        char *tab=strchr(start,'\t');if(tab)*tab=0;
        start[strcspn(start,"\r\n")]=0;
        if(!strcmp(start,"uci"))return column;
        start=tab?tab+1:NULL;column++;
    }
    return -1;
}
static char *field(char *line,int column) {
    char *p=line;
    for(int i=0;i<column;i++) {
        p=strchr(p,'\t');if(!p)return NULL;p++;
    }
    char *end=strchr(p,'\t');if(end)*end=0;
    return p;
}
int main(int argc,char **argv) {
    if(argc<3) {
        fprintf(stderr,"Usage: %s OUTPUT.cbk INPUT.txt [MORE.txt ...]\n",argv[0]);
        return 2;
    }
    init_sliders_attacks();init_leaper_attacks();init_zobrist();
    Bitboard start[12],occ[3];init_board(start,occ);
    uint64_t start_key=generate_zobrist_key(start,WHITE,NO_SQUARE,15);
    Records records={0};size_t accepted=0,rejected=0;
    for(int a=2;a<argc;a++) {
        FILE *in=fopen(argv[a],"r");
        if(!in){perror(argv[a]);free(records.v);return 1;}
        char line[8192];int tsv=-1;
        if(fgets(line,sizeof(line),in)) {
            tsv=tsv_uci_column(line);
            if(tsv<0 && strncmp(line,"eco\t",4) && line[0]!='#') {
                if(process_line(line,&records))accepted++;else rejected++;
            }
            if(tsv<0 && !strncmp(line,"eco\t",4)) {
                fprintf(stderr,"%s has no UCI column; convert SAN PGN with tools/lichess_to_uci.py first\n",argv[a]);
                fclose(in);free(records.v);return 1;
            }
        }
        while(fgets(line,sizeof(line),in)) {
            if(!strchr(line,'\n') && !feof(in)) {
                int c;while((c=fgetc(in))!='\n'&&c!=EOF){}
                rejected++;continue;
            }
            if(line[0]=='#'||line[0]=='\n')continue;
            char *uci=tsv<0?line:field(line,tsv);
            if(uci && process_line(uci,&records))accepted++;else rejected++;
        }
        fclose(in);
    }
    if(!records.n){fprintf(stderr,"No valid opening lines\n");free(records.v);return 1;}
    qsort(records.v,records.n,sizeof(BookRecord),compare_record);
    size_t unique=0;
    for(size_t i=0;i<records.n;i++) {
        if(unique && records.v[unique-1].key==records.v[i].key &&
                     records.v[unique-1].move==records.v[i].move) {
            if(records.v[unique-1].weight<UINT32_MAX)records.v[unique-1].weight++;
        } else records.v[unique++]=records.v[i];
    }
    FILE *out=fopen(argv[1],"wb");
    if(!out){perror(argv[1]);free(records.v);return 1;}
    BookHeader header={{0},start_key,(uint64_t)unique};
    memcpy(header.magic,BOOK_MAGIC,8);
    int ok=fwrite(&header,sizeof(header),1,out)==1 &&
           fwrite(records.v,sizeof(BookRecord),unique,out)==unique;
    if(fclose(out))ok=0;
    printf("book lines accepted=%zu rejected=%zu indexed positions/moves=%zu\n",
           accepted,rejected,unique);
    free(records.v);
    return ok?0:1;
}
