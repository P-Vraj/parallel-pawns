#pragma once
#include "../types.h"

struct Magic {
    Bitboard mask;   // Mask of relevant blockers for this square and piece type
    uint64_t magic;  // Magic number for hashing
    uint8_t shift;   // Right-shift amount = (64 - relevant bits)
};