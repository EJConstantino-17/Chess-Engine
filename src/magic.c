#include "bitboard.h"
#include "magic.h"

static Bitboard bishop_attack_table[5248];
static Bitboard rook_attack_table[102400];

const Bitboard BISHOP_MAGICS[64] = {
    0x0040040822862081ULL, 0x00040810a4108000ULL, 0x2008008400920040ULL, 0x0061050104000008ULL,
    0x8282021010016100ULL, 0x41008210400a0001ULL, 0x03004202104050c0ULL, 0x0022010108410402ULL,
    0x0060400862888605ULL, 0x0006311401040228ULL, 0x0000080801082000ULL, 0x802a082080240100ULL,
    0x1860061210016800ULL, 0x000401016010a810ULL, 0x1000060545201005ULL, 0x21000c2098280819ULL,
    0x2020004242020200ULL, 0x4102100490040101ULL, 0x0114012208001500ULL, 0x0108000682004460ULL,
    0x7809000490401000ULL, 0x420b001601052912ULL, 0x00408c8206100300ULL, 0x2231001041180110ULL,
    0x8010102008a02100ULL, 0x0204201004080084ULL, 0x0410500058008811ULL, 0x480a040008010820ULL,
    0x2194082044002002ULL, 0x2008a20001004200ULL, 0x0040908041041004ULL, 0x0881002200540404ULL,
    0x4001082002082101ULL, 0x0008110408880880ULL, 0x8000404040080200ULL, 0x0200020082180080ULL,
    0x1184440400114100ULL, 0xc220008020110412ULL, 0x4088084040090100ULL, 0x8822104100121080ULL,
    0x100111884008200aULL, 0x2844040288820200ULL, 0x0090901088003010ULL, 0x001000a218000400ULL,
    0x0001102010420204ULL, 0x08414a3483000200ULL, 0x6410849901420400ULL, 0x0201080200901040ULL,
    0x0204880808050002ULL, 0x1001008201210000ULL, 0x016a6300a890040aULL, 0x8049000441108600ULL,
    0x2212002060410044ULL, 0x0100086308020020ULL, 0x0484241408020421ULL, 0x105084028429c085ULL,
    0x004282480801080cULL, 0x081c098488088240ULL, 0x1400000090480820ULL, 0x4444000030208810ULL,
    0x1020142010820200ULL, 0x2234802004018200ULL, 0x00c2040450820a00ULL, 0x0002101021090020ULL
};

const Bitboard ROOK_MAGICS[64] = {
    0xa080041440042080ULL, 0xa840200410004001ULL, 0x0c800c1000200081ULL, 0x0100081001000420ULL,
    0x0200020010080420ULL, 0x03001c0002010008ULL, 0x8480008002000100ULL, 0x2080088004402900ULL,
    0x0000800098204000ULL, 0x2024401000200040ULL, 0x0100802000801000ULL, 0x0120800800801000ULL,
    0x0208808088000400ULL, 0x0002802200800400ULL, 0x2200800100020080ULL, 0x0801000060821100ULL,
    0x0080044006422000ULL, 0x0100808020004000ULL, 0x12108a0010204200ULL, 0x0140848010000802ULL,
    0x0481828014002800ULL, 0x8094004002004100ULL, 0x4010040010010802ULL, 0x0000020008806104ULL,
    0x0100400080208000ULL, 0x2040002120081000ULL, 0x0021200680100081ULL, 0x0020100080080080ULL,
    0x0002000a00200410ULL, 0x0000020080800400ULL, 0x0080088400100102ULL, 0x0080004600042881ULL,
    0x4040008040800020ULL, 0x0440003000200801ULL, 0x0004200011004500ULL, 0x0188020010100100ULL,
    0x0014800401802800ULL, 0x2080040080800200ULL, 0x0124080204001001ULL, 0x0200046502000484ULL,
    0x0480400080088020ULL, 0x1000422010034000ULL, 0x0030200100110040ULL, 0x0000100021010009ULL,
    0x2002080100110004ULL, 0x0202008004008002ULL, 0x0020020004010100ULL, 0x2048440040820001ULL,
    0x0101002200408200ULL, 0x0040802000401080ULL, 0x4008142004410100ULL, 0x02060820c0120200ULL,
    0x0001001004080100ULL, 0x020c020080040080ULL, 0x2935610830022400ULL, 0x0044440041009200ULL,
    0x0280001040802101ULL, 0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
    0x00020030a0244872ULL, 0x0012001008414402ULL, 0x02006104900a0804ULL, 0x0001004081002402ULL
};

MagicEntry bishop_magics[64];
MagicEntry rook_magics[64];

void init_magic_numbers(void) {
    for (int sq = 0; sq < 64; sq++) {
        bishop_magics[sq].magic = BISHOP_MAGICS[sq];
        rook_magics[sq].magic = ROOK_MAGICS[sq];
    }
}

Bitboard mask_bishop_attacks(int sq) {
    Bitboard attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) attacks |= (1ULL << (r * 8 + f));
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) attacks |= (1ULL << (r * 8 + f));

    return attacks;
}

Bitboard mask_rook_attacks(int sq) {
    Bitboard attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    for (r = tr + 1; r <= 6; r++) attacks |= (1ULL << (r * 8 + tf));
    for (r = tr - 1; r >= 1; r--) attacks |= (1ULL << (r * 8 + tf));
    for (f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
    for (f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));

    return attacks;
}

Bitboard bishop_attacks_on_the_fly(int sq, Bitboard block) {
    Bitboard attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }

    return attacks;
}

Bitboard rook_attacks_on_the_fly(int sq, Bitboard block) {
    Bitboard attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    for (r = tr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + tf));
        if (block & (1ULL << (r * 8 + tf))) break;
    }
    for (r = tr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + tf));
        if (block & (1ULL << (r * 8 + tf))) break;
    }
    for (f = tf + 1; f <= 7; f++) {
        attacks |= (1ULL << (tr * 8 + f));
        if (block & (1ULL << (tr * 8 + f))) break;
    }
    for (f = tf - 1; f >= 0; f--) {
        attacks |= (1ULL << (tr * 8 + f));
        if (block & (1ULL << (tr * 8 + f))) break;
    }

    return attacks;
}

Bitboard set_occupancy(int index, int bits_in_mask, Bitboard attack_mask) {
    Bitboard occupancy = 0ULL;
    for (int count = 0; count < bits_in_mask; count++) {
        int square = pop_lsb(&attack_mask);
        if (index & (1 << count)) {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

void init_sliders_attacks(void) {
    Bitboard *bishop_ptr = bishop_attack_table;
    Bitboard *rook_ptr = rook_attack_table;

    for (int sq = 0; sq < 64; sq++) {
        bishop_magics[sq].magic = BISHOP_MAGICS[sq];
        bishop_magics[sq].mask = mask_bishop_attacks(sq);
        bishop_magics[sq].attacks = bishop_ptr;

        int bishop_relevant_bits = __builtin_popcountll(bishop_magics[sq].mask);
        bishop_magics[sq].shift = 64 - bishop_relevant_bits;

        int bishop_occupancy_indices = 1 << bishop_relevant_bits;
        for (int i = 0; i < bishop_occupancy_indices; i++) {
            Bitboard occ = set_occupancy(i, bishop_relevant_bits, bishop_magics[sq].mask);
            size_t magic_index = (size_t)((occ * bishop_magics[sq].magic) >> bishop_magics[sq].shift);
            bishop_magics[sq].attacks[magic_index] = bishop_attacks_on_the_fly(sq, occ);
        }
        bishop_ptr += bishop_occupancy_indices;

        rook_magics[sq].magic = ROOK_MAGICS[sq];
        rook_magics[sq].mask = mask_rook_attacks(sq);
        rook_magics[sq].attacks = rook_ptr;

        int rook_relevant_bits = __builtin_popcountll(rook_magics[sq].mask);
        rook_magics[sq].shift = 64 - rook_relevant_bits;

        int rook_occupancy_indices = 1 << rook_relevant_bits;
        for (int i = 0; i < rook_occupancy_indices; i++) {
            Bitboard occ = set_occupancy(i, rook_relevant_bits, rook_magics[sq].mask);
            size_t magic_index = (size_t)((occ * rook_magics[sq].magic) >> rook_magics[sq].shift);
            rook_magics[sq].attacks[magic_index] = rook_attacks_on_the_fly(sq, occ);
        }
        rook_ptr += rook_occupancy_indices;
    }
}

Bitboard get_bishop_attacks(int sq, Bitboard occupancy) {
    occupancy &= bishop_magics[sq].mask;
    occupancy *= bishop_magics[sq].magic;
    occupancy >>= bishop_magics[sq].shift;
    return bishop_magics[sq].attacks[occupancy];
}

Bitboard get_rook_attacks(int sq, Bitboard occupancy) {
    occupancy &= rook_magics[sq].mask;
    occupancy *= rook_magics[sq].magic;
    occupancy >>= rook_magics[sq].shift;
    return rook_magics[sq].attacks[occupancy];
}

Bitboard get_queen_attacks(int sq, Bitboard occupancy) {
    return get_bishop_attacks(sq, occupancy) | get_rook_attacks(sq, occupancy);
}