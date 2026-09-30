#ifndef CCE_NNUE_NETWORK_FILE_H
#define CCE_NNUE_NETWORK_FILE_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t version;
    uint32_t architecture_hash;
    uint32_t description_length;
    uint64_t file_size;
} NNUEFileInfo;

// 9/30/2026 02:19: Inspect the specific Stockfish network container; no inference.
#ifdef __cplusplus
extern "C" {
#endif
int nnue_inspect_file(const char *path, NNUEFileInfo *out,
                      char *description, size_t description_capacity);
#ifdef __cplusplus
}
#endif
#endif
