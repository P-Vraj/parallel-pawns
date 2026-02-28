#pragma once
#include "checkers.h"
#include "types.h"

// List of moves, with a max size of 256 (max moves in any given position is 218)
struct MoveList {
    std::array<Move, 256> data;
    uint16_t size = 0;

    constexpr void clear() noexcept { size = 0; }
    constexpr void push_back(Move m) noexcept { data[size++] = m; }
    constexpr Move operator[](int i) const noexcept { return data[i]; }
    constexpr Move& operator[](int i) noexcept { return data[i]; }
};