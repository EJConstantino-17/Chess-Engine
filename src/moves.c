#include "../inc/moves.h"

void add_move(MoveList *move_list, Move move) {
    move_list->moves[move_list->count] = move;
    move_list->count++;
}