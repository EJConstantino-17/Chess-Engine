#include "network_file.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define EXPECTED_VERSION 0x6a448afaU
#define EXPECTED_ARCHITECTURE 0xa85b2205U
#define EXPECTED_SIZE UINT64_C(98961994)

static uint32_t read_le32(const unsigned char bytes[4]) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

// 9/30/2026 02:19: Fail closed on truncated or different NNUE containers.
int nnue_inspect_file(const char *path, NNUEFileInfo *out,
                      char *description, size_t description_capacity) {
    if (!path || !out || !description || description_capacity == 0) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    unsigned char header[12];
    int valid = 0;
    if (fread(header, 1, sizeof header, file) != sizeof header) goto done;
    uint32_t version = read_le32(header), hash = read_le32(header + 4);
    uint32_t len = read_le32(header + 8);
    if (version != EXPECTED_VERSION || hash != EXPECTED_ARCHITECTURE ||
        len >= description_capacity || len > 1024) goto done;
    if (fread(description, 1, len, file) != len) goto done;
    description[len] = '\0';
    if (fseek(file, 0, SEEK_END) != 0) goto done;
    long size = ftell(file);
    if (size < 0 || (uint64_t)size != EXPECTED_SIZE) goto done;
    out->version = version;
    out->architecture_hash = hash;
    out->description_length = len;
    out->file_size = (uint64_t)size;
    valid = 1;
done:
    fclose(file);
    return valid;
}
