#include "../inc/see.h"
#include <string.h>
int see_piece_value(int piece) {
    static const int values[6] = {100, 320, 330, 500, 900, 20000};
    return piece >= 0 && piece < 12 ? values[piece % 6] : 0;
}
static Bitboard attackers(int sq, int side, Bitboard b[12], Bitboard occupied) {
    int offset = side * 6;
    return (pawn_attacks[side ^ 1][sq] & b[offset]) |
           (knight_attacks[sq] & b[offset + 1]) |
           (get_bishop_attacks(sq, occupied) & (b[offset + 2] | b[offset + 4])) |
           (get_rook_attacks(sq, occupied) & (b[offset + 3] | b[offset + 4])) |
           (king_attacks[sq] & b[offset + 5]);
}

int see_after_capture(Move move, const Bitboard pieces[12], const Bitboard occupancy[3], int victim) {
    Bitboard b[12], o[3];
    memcpy(b, pieces, sizeof b); memcpy(o, occupancy, sizeof o);
    int gains[32], count = 0, target = MOVE_TARGET(move);
    int occupant = MOVE_PROMOTED(move) ? MOVE_PROMOTED(move) : MOVE_PIECE(move);
    int side = (MOVE_PIECE(move) / 6) ^ 1;
    gains[0] = see_piece_value(victim);
    if (MOVE_PROMOTED(move)) gains[0] += see_piece_value(occupant) - 100;
    Bitboard target_bit = UINT64_C(1) << target;
    while (count < 30) {
        Bitboard candidates = attackers(target, side, b, o[BOTH]);
        int found = 0, promoted = 0, selected = -1;
        for (int type = 0; type < 6 && !found; ++type) {
            int piece = side * 6 + type;
            Bitboard available = candidates & b[piece];
            while (available) {
                int sq = get_lsb_index(available); available &= available - 1;
                Bitboard bit = UINT64_C(1) << sq;
                int result = piece;
                if (type == 0 && (target / 8 == 0 || target / 8 == 7)) result = side * 6 + 4;
                b[occupant] ^= target_bit; b[piece] ^= bit; b[result] ^= target_bit;
                o[side] ^= bit | target_bit; o[side ^ 1] ^= target_bit; o[BOTH] ^= bit;
                int king = get_lsb_index(b[side * 6 + 5]);
                int legal = king >= 0 && !is_square_attacked(king, b, o, side ^ 1);
                if (legal) { selected = result; promoted = result != piece; found = 1; break; }
                b[result] ^= target_bit; b[piece] ^= bit; b[occupant] ^= target_bit;
                o[side] ^= bit | target_bit; o[side ^ 1] ^= target_bit; o[BOTH] ^= bit;
            }
        }
        if (!found) break;
        ++count;
        gains[count] = see_piece_value(occupant) + (promoted ? 800 : 0) - gains[count - 1];
        occupant = selected; side ^= 1;
        if (occupant % 6 == 5) break;
    }
    while (count) {
        int previous = gains[count - 1];
        gains[count - 1] = -((-previous > gains[count]) ? -previous : gains[count]);
        --count;
    }
    return gains[0];
}
