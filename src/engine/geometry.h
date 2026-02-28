#pragma once
#include "types.h"

constexpr inline std::array<Direction, 8> kDirs = {
    Direction::North, Direction::South, Direction::East, Direction::West,
    Direction::NorthEast, Direction::NorthWest, Direction::SouthEast, Direction::SouthWest
};

constexpr inline bool can_step(Square sq, Direction dir) noexcept;

constexpr inline bool aligned_dir(Square from, Square to, Direction& out) noexcept;

// Returns bitboard of squares strictly between a and b (excluding a and b), or 0 if a and b are not aligned.
constexpr inline Bitboard between(Square a, Square b) noexcept;