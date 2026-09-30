#include "cce_nnue.h"
#include "network_file.h"
#include "sha256.h"
#include "stockfish/src/attacks.h"
#include "stockfish/src/position.h"
#include "stockfish/src/nnue/network.h"
#include "stockfish/src/nnue/nnue_accumulator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>


extern "C" {
uint64_t cce_nnue_accumulator_paths[4] = {};
}
namespace {
using namespace Stockfish;
using namespace Stockfish::Eval::NNUE;
std::unique_ptr<Network> net;
std::unique_ptr<AccumulatorCaches> cache;
std::unique_ptr<AccumulatorStack> accum;
Position pos;
std::array<StateInfo, AccumulatorStack::MaxSize> states;
std::array<Move, AccumulatorStack::MaxSize> moves;
struct Frame { unsigned generation = 0; unsigned depth = 0; uint32_t move = 0; };
std::array<Frame, 256> frames;
unsigned generation = 1, depth = 0;
uint64_t updates = 0;
CCENNUEStats performance = {};
bool active = false, valid = false, initialized = false;
std::string error;
constexpr Piece mapping[12] = {W_PAWN,W_KNIGHT,W_BISHOP,W_ROOK,W_QUEEN,W_KING,
                               B_PAWN,B_KNIGHT,B_BISHOP,B_ROOK,B_QUEEN,B_KING};
std::string fen(const uint64_t b[12], int side, int ep, int castle) {
    constexpr char letters[] = "PNBRQKpnbrqk";
    std::ostringstream out;
    for (int rank=7; rank>=0; --rank) {
        int empty=0;
        for (int file=0; file<8; ++file) {
            int found=-1;
            for (int p=0;p<12;++p) if (b[p] & (UINT64_C(1) << (rank*8+file))) { found=p; break; }
            if (found<0) ++empty;
            else { if(empty) out<<empty; empty=0; out<<letters[found]; }
        }
        if(empty) out<<empty;
        if(rank) out<<'/';
    }
    out<<(side ? " b " : " w ");
    if (!castle) out<<'-';
    else { if(castle&1) out<<'K'; if(castle&2) out<<'Q'; if(castle&4) out<<'k'; if(castle&8) out<<'q'; }
    out<<' ';
    if(ep>=0 && ep<64) out<<char('a'+ep%8)<<char('1'+ep/8); else out<<'-';
    out<<" 0 1";
    return out.str();
}
bool same(const uint64_t b[12], int side) {
    if(!valid || int(pos.side_to_move())!=side) return false;
    for(int p=0;p<12;++p) if(pos.pieces(color_of(mapping[p]),type_of(mapping[p]))!=b[p]) return false;
    return true;
}
bool sync(const uint64_t b[12], int side, int ep, int castle, bool force=false) {
    if(!force && same(b,side)) return true;
#ifdef CCE_NNUE_PROFILE
    ++performance.resyncs;
#endif
    ++generation; depth=0; valid=false; accum->reset();
    auto err=pos.set(fen(b,side,ep,castle),false,&states[0]);
    if(err) { error=err->what(); return false; }
    valid=true;
    return true;
}
Move translate(uint32_t m) {
    Square from=Square(m&63), to=Square((m>>6)&63);
    if(m&(1u<<23)) return Move::make<CASTLING>(from,Square((int(to)/8)*8+(int(to)%8==6 ? 7:0)));
    if(m&(1u<<22)) return Move::make<EN_PASSANT>(from,to);
    unsigned promoted=(m>>16)&15;
    if(promoted) return Move::make<PROMOTION>(from,to,PieceType(promoted%6+1));
    return Move(from,to);
}
void init_tables() {
    if(!initialized) { Attacks::init(); Position::init(); initialized=true; }
}
}

extern "C" {

void cce_nnue_reset_stats(void) {
    performance = {};
    std::fill(std::begin(cce_nnue_accumulator_paths), std::end(cce_nnue_accumulator_paths), 0);
}
CCENNUEStats cce_nnue_get_stats(void) {
    CCENNUEStats result = performance;
    result.cached = cce_nnue_accumulator_paths[0];
    result.incremental = cce_nnue_accumulator_paths[1];
    result.refresh = cce_nnue_accumulator_paths[2];
    result.hybrid = cce_nnue_accumulator_paths[3];
    return result;
}
int cce_nnue_profile_enabled(void) {
#ifdef CCE_NNUE_PROFILE
    return 1;
#else
    return 0;
#endif
}
const char *cce_nnue_build_profile(void) {
#if defined(USE_AVX2)
    return "AVX2";
#elif defined(USE_NEON)
    return "NEON";
#else
    return "SCALAR";
#endif
}
int cce_nnue_load(const char *path) {
    if(!path || !*path) { error="empty network path"; return 0; }
    NNUEFileInfo info; char description[1025];
    if(!nnue_inspect_file(path,&info,description,sizeof description)) { error="missing, truncated, or incompatible NNUE container"; return 0; }
    if(cce_file_sha256(path)!="134a887f4c8ff7bf7284177a3b3fc6ff9cef95ba89eb8db3079a8e507f7126af") {
        error="network SHA-256 mismatch"; return 0;
    }
    try {
        init_tables();
        auto candidate=std::make_unique<Network>();
        EvalFile metadata;
        candidate->load_external({},path,metadata);
        if(!metadata.current) { error="NNUE parameters, feature/layer hashes, or end-of-file validation failed"; return 0; }
        auto candidate_cache=std::make_unique<AccumulatorCaches>(*candidate);
        auto candidate_accum=std::make_unique<AccumulatorStack>();
        net=std::move(candidate); cache=std::move(candidate_cache); accum=std::move(candidate_accum);
        active=true; valid=false; depth=0; ++generation; error.clear();
        return 1;
    } catch(const std::exception& ex) { error=ex.what(); return 0; }
}
void cce_nnue_init(void) {
    static bool once=false;
    if(once) return;
    once=true;
    const char *backend=std::getenv("CCE_EVAL");
    if(backend && std::string(backend)=="pesto") return;
    const char *path=std::getenv("CCE_NNUE_FILE");
    if(!path) path="nn-134a887f4c8f.nnue";
    if(!cce_nnue_load(path)) std::fprintf(stderr,"CCE: NNUE unavailable (%s); using PeSTO.\n",error.c_str());
}
int cce_nnue_enabled(void) { return active && bool(net); }
void cce_nnue_enable(int enable) { active=enable && bool(net); cce_nnue_reset(); }
void cce_nnue_reset(void) { valid=false; depth=0; ++generation; if(accum) accum->reset(); }
const char *cce_nnue_error(void) { return error.c_str(); }
uint64_t cce_nnue_updates(void) { return updates; }
int cce_nnue_raw(const uint64_t b[12], int side, int fresh) {
    if(!net) return 0;
    if(fresh) {
        Position test; StateInfo state;
        if(test.set(fen(b,side,-1,0),false,&state)) return 0;
        auto a=std::make_unique<AccumulatorStack>();
        auto c=std::make_unique<AccumulatorCaches>(*net);
        return net->evaluate(test,*a,*c);
    }
    if(!sync(b,side,-1,0)) return 0;
#ifdef CCE_NNUE_PROFILE
    ++performance.evaluations;
#endif
    return net->evaluate(pos,*accum,*cache);
}
int cce_nnue_validate(const uint64_t b[12],int side) {
    if(!net || !sync(b,side,-1,0)) return 0;
    int incremental=net->evaluate(pos,*accum,*cache);
    Position test; StateInfo state;
    if(test.set(fen(b,side,-1,0),false,&state)) return 0;
    auto fresh=std::make_unique<AccumulatorStack>();
    auto cold=std::make_unique<AccumulatorCaches>(*net);
    int rebuilt=net->evaluate(test,*fresh,*cold);
    return incremental==rebuilt && accum->latest().accumulation==fresh->latest().accumulation
        && accum->latest().psqtAccumulation==fresh->latest().psqtAccumulation;
}

int cce_nnue_evaluate(const uint64_t b[12], int side) {
    int raw=cce_nnue_raw(b,side,0);
    int material=pos.count<PAWN>()+3*pos.count<KNIGHT>()+3*pos.count<BISHOP>()+5*pos.count<ROOK>()+9*pos.count<QUEEN>();
    double m=std::clamp(material,17,78)/58.0;
    double a=(((-142.72052667*m+372.35176398)*m-340.71073572)*m)+415.23490212;
    return std::clamp(int(std::round(100.0*raw/a)),-20000,20000);
}

void cce_nnue_prepare(uint32_t m,const uint64_t b[12],int side,int ep,int castle,int ply) {
    if(!cce_nnue_enabled() || ply<0 || ply>=256) return;
    frames[ply]={};
    int rights=0;
    if(valid) for(int bit=1;bit<=8;bit<<=1) if(pos.can_castle(CastlingRights(bit))) rights|=bit;
    if(!sync(b,side,ep,castle,valid && rights!=castle)) return;
    if(depth+1>=AccumulatorStack::MaxSize || (ply==0 && depth)) {
        if(!sync(b,side,ep,castle,true)) return;
    }
    Move sm=translate(m);
    // Castling rights and EP are absent when an evaluation initialized the root.
    if((sm.type_of()==CASTLING && !pos.can_castle(side ? BLACK_CASTLING : WHITE_CASTLING))
        || (sm.type_of()==EN_PASSANT && pos.ep_square()!=sm.to_sq())) {
        if(!sync(b,side,ep,castle,true)) return;
    }
    frames[ply]={};
}
void cce_nnue_commit(uint32_t m,int ply) {
    if(!cce_nnue_enabled() || !valid || ply<0 || ply>=256) return;
    Move sm=translate(m);
    auto& dirties=accum->push();
    ++depth; moves[depth]=sm;
    pos.do_move(sm,states[depth],pos.gives_check(sm),dirties,nullptr,nullptr);
    frames[ply]={generation,depth,m};
    ++updates;
#ifdef CCE_NNUE_PROFILE
    ++performance.commits;
#endif
}
void cce_nnue_unmake(uint32_t m,int ply) {
    if(!cce_nnue_enabled() || ply<0 || ply>=256) return;
    auto f=frames[ply]; frames[ply]={};
    if(!f.generation) return;
    if(f.generation!=generation || f.move!=m || f.depth!=depth || !depth) { cce_nnue_reset(); return; }
    pos.undo_move(moves[depth]); accum->pop(); --depth;
}
void cce_nnue_null(const uint64_t b[12],int side,int ep,int castle) {
    if(!cce_nnue_enabled() || !sync(b,side,ep,castle)) return;
    if(depth+1>=AccumulatorStack::MaxSize && !sync(b,side,ep,castle,true)) return;
    accum->push()={}; ++depth; moves[depth]=Move::null(); pos.do_null_move(states[depth]);
}
void cce_nnue_undo_null(void) {
    if(!cce_nnue_enabled()) return;
    if(!depth || moves[depth]!=Move::null()) { cce_nnue_reset(); return; }
    pos.undo_null_move(); accum->pop(); --depth;
}
}
