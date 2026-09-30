#ifndef CCE_NNUE_H
#define CCE_NNUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef CCE_NNUE

typedef struct {
    uint64_t evaluations, resyncs, commits;
    uint64_t cached, incremental, refresh, hybrid;
} CCENNUEStats;
void cce_nnue_reset_stats(void);
CCENNUEStats cce_nnue_get_stats(void);
int cce_nnue_profile_enabled(void);
const char *cce_nnue_build_profile(void);
int cce_nnue_load(const char *path);
void cce_nnue_init(void);
int cce_nnue_enabled(void);
void cce_nnue_enable(int enabled);
void cce_nnue_reset(void);
int cce_nnue_evaluate(const uint64_t pieces[12], int side);
int cce_nnue_raw(const uint64_t pieces[12], int side, int fresh);
void cce_nnue_prepare(uint32_t move, const uint64_t pieces[12], int side, int ep, int castle, int ply);
void cce_nnue_commit(uint32_t move, int ply);
void cce_nnue_unmake(uint32_t move, int ply);
void cce_nnue_null(const uint64_t pieces[12], int side, int ep, int castle);
void cce_nnue_undo_null(void);
const char *cce_nnue_error(void);
uint64_t cce_nnue_updates(void);
int cce_nnue_validate(const uint64_t pieces[12], int side);
#else
static inline void cce_nnue_init(void) {}
static inline int cce_nnue_enabled(void) { return 0; }
static inline void cce_nnue_prepare(uint32_t m, const uint64_t b[12], int s, int e, int c, int p) {
    (void)m; (void)b; (void)s; (void)e; (void)c; (void)p;
}
static inline void cce_nnue_commit(uint32_t m, int p) { (void)m; (void)p; }
static inline void cce_nnue_unmake(uint32_t m, int p) { (void)m; (void)p; }
static inline void cce_nnue_null(const uint64_t b[12], int s, int e, int c) { (void)b; (void)s; (void)e; (void)c; }
static inline void cce_nnue_undo_null(void) {}
#endif
#ifdef __cplusplus
}
#endif
#endif
