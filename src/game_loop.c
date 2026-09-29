
#include "game_loop.h"
#include "search.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void square_to_uci(int square, char *output);
void move_to_uci(int from_sq, int to_sq, char *move_str);

static int char_to_promo_piece(char c, int side_to_move) {
    switch (tolower((unsigned char)c)) {
        case 'q': return (side_to_move == WHITE) ? Q : q;
        case 'r': return (side_to_move == WHITE) ? R : r;
        case 'b': return (side_to_move == WHITE) ? B : b;
        case 'n': return (side_to_move == WHITE) ? N : n;
        default:  return -1;
    }
}

int uci_to_move(const char *uci, Bitboard pieces[12], Bitboard occupancy[3],
                int side_to_move, int ep_square, int castle_rights,
                Move *out_move) {
    size_t len = strlen(uci);
    if (len < 4) return 0;

    int from_sq = (uci[0] - 'a') + (uci[1] - '1') * 8;
    int to_sq   = (uci[2] - 'a') + (uci[3] - '1') * 8;
    if (uci[0] < 'a' || uci[0] > 'h' || uci[2] < 'a' || uci[2] > 'h' ||
        uci[1] < '1' || uci[1] > '8' || uci[3] < '1' || uci[3] > '8') return 0;

    int wants_promo  = (len >= 5);
    int promo_target = wants_promo ? char_to_promo_piece(uci[4], side_to_move) : -1;
    if (wants_promo && promo_target == -1) return 0;

    MoveList moves;
    generate_all_moves(&moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    for (int i = 0; i < moves.count; i++) {
        Move m = moves.moves[i];
        if (MOVE_SRC(m) != (unsigned)from_sq || MOVE_TARGET(m) != (unsigned)to_sq) continue;

        int m_promoted = MOVE_PROMOTED(m); // 0 means "not a promotion" (P/p are never a promotion target)

        if (wants_promo) {
            if (m_promoted != promo_target) continue;   // e.g. typed "q" but this candidate promotes to N
        } else {
            if (m_promoted != 0) continue;               // this square pair needs a promotion letter
        }

        *out_move = m;
        return 1;
    }
    return 0; // no legal move matches — illegal move or a typo
}

// Same probe-and-restore pattern as negamax()'s inner loop, used only
// to answer "does this side have any legal move at all" for end-of-game
// detection in the interactive loop.
static int side_has_legal_move(Bitboard pieces[12], Bitboard occupancy[3],
                                int side_to_move, int ep_square, int castle_rights) {
    MoveList moves;
    generate_all_moves(&moves, pieces, occupancy, side_to_move, ep_square, castle_rights);

    for (int i = 0; i < moves.count; i++) {
        Bitboard pieces_copy[12], occ_copy[3];
        memcpy(pieces_copy, pieces, sizeof(pieces_copy));
        memcpy(occ_copy, occupancy, sizeof(occ_copy));
        int side_copy = side_to_move, ep_copy = ep_square, castle_copy = castle_rights;

        int legal = make_move(moves.moves[i], pieces, occupancy,
                               &side_copy, &ep_copy, &castle_copy, 0);

        memcpy(pieces, pieces_copy, sizeof(pieces_copy));
        memcpy(occupancy, occ_copy, sizeof(occ_copy));

        if (legal) return 1;
    }
    return 0;
}

void play_game_loop_with_state(Bitboard pieces[12], Bitboard occupancy[3],
                                int side_to_move, int ep_square, int castle_rights,
                                int engine_side, int search_depth, SearchState *history) {
    char user_input[16];
    char engine_move_str[6];

    printf("\n==================================================\n");
    printf("         INTERACTIVE GAME LOOP ENABLED            \n");
    printf("==================================================\n");
    printf("Enter moves in UCI format (e2e4, g1f3, e7e8q). Type 'quit' to exit.\n");

    print_board(pieces);

    while (1) {
        if (!side_has_legal_move(pieces, occupancy, side_to_move, ep_square, castle_rights)) {
            int own_king  = (side_to_move == WHITE) ? K : k;
            int king_sq   = get_lsb_index(pieces[own_king]);
            int enemy     = (side_to_move == WHITE) ? BLACK : WHITE;
            if (is_square_attacked(king_sq, pieces, occupancy, enemy)) {
                printf("\nCheckmate — %s wins.\n", side_to_move == WHITE ? "Black" : "White");
            } else {
                printf("\nStalemate — draw.\n");
            }
            break;
        }

        // claims belong to the side to move; checkmate/stalemate
        // above have already been checked. The engine elects to claim promptly.
        int can_claim_draw = search_draw_claim_available(pieces, occupancy,
                                   side_to_move, ep_square, castle_rights, history);
        if (can_claim_draw && side_to_move == engine_side) {
            puts("Engine claims draw: threefold repetition or fifty-move rule.");
            break;
        }

        if (side_to_move == engine_side) {
            printf("\n[ENGINE'S TURN]\nEngine is thinking...\n");

            Move best = search_best_move_with_state(pieces, occupancy, side_to_move,
                                                     ep_square, castle_rights, search_depth, history);
            if (!best) { puts("No legal move or draw available."); break; }

            move_to_uci(MOVE_SRC(best), MOVE_TARGET(best), engine_move_str);
            printf("Engine played: %s\n", engine_move_str);

            if (!make_move(best, pieces, occupancy, &side_to_move, &ep_square, &castle_rights, 0)) {
                puts("Engine produced an illegal move."); break;
            }
            // only committed game moves enter the persistent history.
            search_state_record_move(history, best, pieces, side_to_move, ep_square, castle_rights);

            print_board(pieces);
        } else {
            if (can_claim_draw) puts("Draw claim available. Type 'draw' to claim, or enter a move.");
            printf("\n[YOUR TURN] Move: ");

            if (scanf("%15s", user_input) != 1) break;

            if (strcmp(user_input, "quit") == 0) {
                printf("Thanks for playing!\n");
                break;
            }
            if (strcmp(user_input, "draw") == 0 && can_claim_draw) {
                puts("Draw claimed: threefold repetition or fifty-move rule.");
                break;
            }

            Move user_move;
            if (!uci_to_move(user_input, pieces, occupancy, side_to_move,
                              ep_square, castle_rights, &user_move)) {
                printf("Illegal or unrecognized move: %s. Try again.\n", user_input);
                continue;
            }

            if (!make_move(user_move, pieces, occupancy, &side_to_move,
                            &ep_square, &castle_rights, 0)) {
                printf("That move leaves your king in check. Try again.\n");
                continue;
            }

            search_state_record_move(history, user_move, pieces, side_to_move, ep_square, castle_rights);

            print_board(pieces);
        }
    }
}

// legacy game entry starts without earlier moves or a FEN halfmove field.
void play_game_loop(Bitboard pieces[12], Bitboard occupancy[3],
                     int side_to_move, int ep_square, int castle_rights,
                     int engine_side, int search_depth) {
    SearchState history;
    search_state_reset(&history, pieces, side_to_move, ep_square, castle_rights, 0);
    play_game_loop_with_state(pieces, occupancy, side_to_move, ep_square,
                              castle_rights, engine_side, search_depth, &history);
}
