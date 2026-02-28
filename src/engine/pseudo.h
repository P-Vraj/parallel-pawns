#pragma once
#include "checkers.h"
#include "types.h"

// List of moves, with a max size of 256 (max moves in any given position is known to be 218)
struct MoveList {
    std::array<Move, 256> data;
    uint8_t size = 0;

    constexpr void clear() noexcept { size = 0; }
    constexpr uint8_t size() const noexcept { return size; }
    constexpr bool empty() const noexcept { return size == 0; }
    constexpr void push_back(Move m) noexcept { data[size++] = m; }
    constexpr Move back() const noexcept { return data[size - 1]; }
    void pop_back() noexcept { --size; }
    constexpr Move operator[](int i) const noexcept { return data[i]; }
    constexpr Move& operator[](int i) noexcept { return data[i]; }
};