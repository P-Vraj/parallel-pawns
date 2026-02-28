#pragma once
#include "types.h"

namespace geom {

namespace internal {
constexpr inline Bitboard kFileA = 0x0101010101010101ULL;
constexpr inline Bitboard kFileH = 0x8080808080808080ULL;
constexpr inline Bitboard kRank1 = 0x00000000000000FFULL;
constexpr inline Bitboard kRank8 = 0xFF00000000000000ULL;
};  // namespace internal

// clang-format off
constexpr inline std::array<Direction, 8> kDirections = {
    Direction::North, Direction::South, Direction::East, Direction::West,
    Direction::NorthEast, Direction::NorthWest, Direction::SouthEast, Direction::SouthWest
};
// clang-format on

extern std::array<std::array<Bitboard, 64>, 64> line_table;
extern std::array<std::array<Bitboard, 64>, 64> between_table;
extern std::array<std::array<Bitboard, 64>, 64> ray_pass_table;

// Initializes the line, between, and ray_pass tables.
void init_geometry_tables() noexcept;
// Checks if a piece on the given square can step in the given direction without going off the board
constexpr bool can_step(Square sq, Direction dir) noexcept {
    const Bitboard bb = bitboard(sq);
    switch (dir) {
        case Direction::North:
            return (bb & internal::kRank8) == 0;
        case Direction::South:
            return (bb & internal::kRank1) == 0;
        case Direction::East:
            return (bb & internal::kFileH) == 0;
        case Direction::West:
            return (bb & internal::kFileA) == 0;
        case Direction::NorthEast:
            return (bb & (internal::kRank8 | internal::kFileH)) == 0;
        case Direction::NorthWest:
            return (bb & (internal::kRank8 | internal::kFileA)) == 0;
        case Direction::SouthEast:
            return (bb & (internal::kRank1 | internal::kFileH)) == 0;
        case Direction::SouthWest:
            return (bb & (internal::kRank1 | internal::kFileA)) == 0;
        default:
            return false;
    }
}
// Returns bitboard of squares strictly between a and b (excluding a and b), or 0 if a and b are not aligned.
inline Bitboard between(Square a, Square b) noexcept {
    return between_table[to_underlying(a)][to_underlying(b)];
}
// Returns bitboard of squares between a and b (including b), or 0 if a and b are not aligned.
inline Bitboard between_or_to(Square a, Square b) noexcept {
    return between_table[to_underlying(a)][to_underlying(b)] | bitboard(b);
}
// Returns bitboard of squares on the full line with a to b (including a and b), or 0 if a and b are not aligned.
inline Bitboard line(Square a, Square b) noexcept {
    return line_table[to_underlying(a)][to_underlying(b)];
}
// Returns bitboard of squares on the ray from a past b, or 0 if a and b are not aligned.
inline Bitboard ray_pass(Square a, Square b) noexcept {
    return ray_pass_table[to_underlying(a)][to_underlying(b)];
}

};  // namespace geom