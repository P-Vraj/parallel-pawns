#pragma once
#include "../types.h"

struct Magic {
    Bitboard mask;      // mask of relevant blockers for this square and piece type
    uint64_t magic;     // magic number for hashing
    uint8_t shift;      // right-shift amount = (64 - relevant bits)
};