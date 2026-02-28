#pragma once
#include "types.h"

// clang-format off
constexpr inline std::array<Direction, 8> kDirs = {
    Direction::North, Direction::South, Direction::East, Direction::West,
    Direction::NorthEast, Direction::NorthWest, Direction::SouthEast, Direction::SouthWest
};
// clang-format on

namespace geom {

// Checks if a piece on the given square can step in the given direction without going off the board
constexpr inline bool can_step(Square sq, Direction dir) noexcept;

// Initializes the line, between, and ray_pass tables.
void init_geometry_tables() noexcept;
// Returns bitboard of squares strictly between a and b (excluding a and b), or 0 if a and b are not aligned.
inline Bitboard between(Square a, Square b) noexcept;
// Returns bitboard of squares between a and b (including b), or 0 if a and b are not aligned.
inline Bitboard between_or_to(Square a, Square b) noexcept;
// Returns bitboard of squares on the full line with a to b (including a and b), or 0 if a and b are not aligned.
inline Bitboard line(Square a, Square b) noexcept;
// Returns bitboard of squares on the ray from a past b, or 0 if a and b are not aligned.
inline Bitboard ray_pass(Square a, Square b) noexcept;

};  // namespace geom