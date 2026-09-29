#include "opening_book.h"
#include "game_loop.h"
#include "tt.h"
#include "book_index.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static uint64_t rng_state;
// bundled fallback if the GUI starts us from an inaccessible
// working directory; opening_book.txt can override these lines.
static const char *const bundled_lines[] = {
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6",
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4",
    "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6",
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6",
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4",
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7",
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6",
    "g1f3 d7d5 d2d4 g8f6 c2c4 e7e6"
};
static uint64_t random_word(void) {
    if (!rng_state) {
        struct timespec now;
#ifdef __ANDROID__
        // timespec_get requires API 29; clock_gettime works
        // with the API 26 target and still gives a per-process random seed.
        clock_gettime(CLOCK_REALTIME, &now);
#else
        timespec_get(&now, TIME_UTC);
#endif
        rng_state = ((uint64_t)now.tv_sec << 32) ^ (uint64_t)now.tv_nsec ^
                    (uint64_t)(uintptr_t)&rng_state;
        if (!rng_state) rng_state = 0x9e3779b97f4a7c15ULL;
    }
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

static int legal_candidate(const char *text, Bitboard pieces[12], Bitboard occupancy[3],
                           int side, int ep, int castle, Move *out) {
    if (!uci_to_move(text, pieces, occupancy, side, ep, castle, out)) return 0;
    Bitboard copy[12], occ_copy[3];
    memcpy(copy, pieces, sizeof(copy));
    memcpy(occ_copy, occupancy, sizeof(occ_copy));
    return make_move(*out, copy, occ_copy, &side, &ep, &castle, 0);
}

Move opening_book_pick_index(const char *path, Bitboard pieces[12],
                             Bitboard occupancy[3], int side, int ep,
                             int castle, int *loaded) {
    *loaded=0;
    FILE *file=fopen(path,"rb");
    if(!file)return 0;
    BookHeader header;
    Bitboard start[12], start_occ[3];init_board(start,start_occ);
    uint64_t start_key=generate_zobrist_key(start,WHITE,NO_SQUARE,15);
    if(fread(&header,sizeof(header),1,file)!=1 ||
       memcmp(header.magic,BOOK_MAGIC,8) || header.start_key!=start_key ||
       header.count>10000000ULL ||
       fseek(file,0,SEEK_END) ||
       ftell(file)<0 || (uint64_t)ftell(file)!=sizeof(header)+
                                            header.count*sizeof(BookRecord)) {
        fclose(file);
        return 0; // Wrong format/hash seed or truncated index: use text book.
    }
    *loaded=1;
    uint64_t key=generate_zobrist_key(pieces,side,ep,castle);
    size_t lo=0,hi=(size_t)header.count;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        BookRecord entry;
        if(fseek(file,(long)(sizeof(header)+mid*sizeof(entry)),SEEK_SET) ||
           fread(&entry,sizeof(entry),1,file)!=1) {
            *loaded=0;fclose(file);return 0;
        }
        if(entry.key<key)lo=mid+1;else hi=mid;
    }
    uint64_t total_weight=0;
    Move picked=0;
    MoveList legal;
    generate_all_moves(&legal,pieces,occupancy,side,ep,castle);
    for(size_t i=lo;i<(size_t)header.count;i++) {
        BookRecord entry;
        if(fseek(file,(long)(sizeof(header)+i*sizeof(entry)),SEEK_SET) ||
           fread(&entry,sizeof(entry),1,file)!=1)break;
        if(entry.key!=key)break;
        if(!entry.weight)continue;
        int found=0;
        for(int j=0;j<legal.count;j++)if(legal.moves[j]==entry.move){found=1;break;}
        if(!found)continue;
        Bitboard copy[12],occ_copy[3];
        memcpy(copy,pieces,sizeof(copy));memcpy(occ_copy,occupancy,sizeof(occ_copy));
        int next_side=side,next_ep=ep,next_castle=castle;
        if(!make_move(entry.move,copy,occ_copy,&next_side,&next_ep,&next_castle,0))continue;
        total_weight+=entry.weight;
        if(random_word()%total_weight<entry.weight)picked=entry.move;
    }
    fclose(file);
    return picked;
}

static void process_line(char line[512], const SearchState *history,
                         Bitboard initial[12], Bitboard initial_occ[3],
                         Bitboard pieces[12], Bitboard occupancy[3],
                         int side, int ep, int castle, Move *chosen, unsigned *matches) {
    Bitboard board[12], occ[3];
    memcpy(board, initial, sizeof(board));
    memcpy(occ, initial_occ, sizeof(occ));
    int turn = WHITE, sq = NO_SQUARE, rights = 15;
    char *token = strtok(line, " \r\n\t");
    for (int ply = 0; token && ply < history->count; ++ply) {
        Move m = 0;
        if (ply == history->count - 1) {
            if (legal_candidate(token, pieces, occupancy, side, ep, castle, &m)) {
                if (++*matches == 1 || random_word() % *matches == 0) *chosen = m;
            }
            return;
        }
        if (!uci_to_move(token, board, occ, turn, sq, rights, &m) ||
            !make_move(m, board, occ, &turn, &sq, &rights, 0) ||
            generate_zobrist_key(board, turn, sq, rights) != history->keys[ply + 1])
            return;
        token = strtok(NULL, " \r\n\t");
    }
}

Move opening_book_pick(const char *path, const SearchState *history,
                       Bitboard pieces[12], Bitboard occupancy[3],
                       int side, int ep, int castle) {
    if (!history || history->count < 1 || history->count > 20) return 0;
    FILE *file = fopen(path && *path ? path : "opening_book.txt", "r");
    Bitboard initial[12], initial_occ[3];
    init_board(initial, initial_occ);
    const uint64_t start_key = generate_zobrist_key(initial, WHITE, NO_SQUARE, 15);
    if (history->keys[0] != start_key) { if (file) fclose(file); return 0; }

    Move chosen = 0;
    unsigned matches = 0;
    char line[512];
    while (file && fgets(line, sizeof(line), file)) {
        if (!strchr(line, '\n') && !feof(file)) { // Discard an oversized line.
            int c; while ((c = fgetc(file)) != '\n' && c != EOF) {}
            continue;
        }
        if (line[0] == '#' || line[0] == '\n') continue;
        // Reservoir sampling weights repeated lines without depending on order.
        process_line(line, history, initial, initial_occ, pieces, occupancy,
                     side, ep, castle, &chosen, &matches);
    }
    if (file) fclose(file);
    else for (size_t i=0; i<sizeof(bundled_lines)/sizeof(bundled_lines[0]); ++i) {
        snprintf(line,sizeof(line),"%s",bundled_lines[i]);
        process_line(line, history, initial, initial_occ, pieces, occupancy,
                     side, ep, castle, &chosen, &matches);
    }
    return chosen;
}
