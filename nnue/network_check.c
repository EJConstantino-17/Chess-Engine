#include "network_file.h"
#include <stdio.h>

int main(int argc, char **argv) {
    NNUEFileInfo info;
    char description[1025];
    if (argc != 2 || !nnue_inspect_file(argv[1], &info,
                                         description, sizeof description)) {
        fputs("Expected Stockfish nn-134a887f4c8f.nnue container not found or invalid.\n", stderr);
        return 1;
    }
    printf("NNUE container: %llu bytes, version 0x%08x, architecture 0x%08x\n%s\n",
           (unsigned long long)info.file_size, info.version,
           info.architecture_hash, description);
    puts("Inference unavailable: this utility validates the container header only.");
    return 0;
}
