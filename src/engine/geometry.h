#pragma once
#include "types.h"

namespace geom {

// clang-format off
inline constexpr std::array<Direction, 8> kDirections = {
    Direction::North, Direction::South, Direction::East, Direction::West,
    Direction::NorthEast, Direction::NorthWest, Direction::SouthEast, Direction::SouthWest
};
// clang-format on

// Map Direction to an index from 0-7 for table lookups
constexpr size_t direction_index(Direction dir) noexcept {
    switch (dir) {
        case Direction::North:
            return 0;
        case Direction::South:
            return 1;
        case Direction::East:
            return 2;
        case Direction::West:
            return 3;
        case Direction::NorthEast:
            return 4;
        case Direction::NorthWest:
            return 5;
        case Direction::SouthEast:
            return 6;
        case Direction::SouthWest:
            return 7;
        default:
            return 8;  // Invalid direction index
    }
}
constexpr std::array<std::array<Square, 8>, 64> make_step_table() noexcept {
    std::array<std::array<Square, 8>, 64> stepTable{};

    constexpr auto step_in_direction = [](Square sq, Direction dir) noexcept {
        const int s = static_cast<int>(to_underlying(sq));
        const int d = static_cast<int>(to_underlying(dir));  // NOLINT(bugprone-signed-char-misuse): Safe/intentional
        const int file = s & 7;
        // Prevent horizontal wraparound
        if ((d == +1 || d == +9 || d == -7) && file == 7)
            return Square::None;
        if ((d == -1 || d == +7 || d == -9) && file == 0)
            return Square::None;
        const int next_step = s + d;
        return (next_step >= 0 && next_step < 64) ? static_cast<Square>(next_step) : Square::None;
    };

    for (auto& row : stepTable) {
        row.fill(Square::None);
    }

    for (int s = 0; s < 64; ++s) {
        const auto sq = static_cast<Square>(s);
        for (const Direction dir : kDirections) {
            stepTable[s][direction_index(dir)] = step_in_direction(sq, dir);
        }
    }

    return stepTable;
}

inline constexpr std::array<std::array<Square, 8>, 64> kStepTable = make_step_table();
// Steps from the given square in the given direction, or returns Square::None if stepping goes off the board.
constexpr Square step(Square sq, Direction dir) noexcept {
    return kStepTable[to_underlying(sq)][direction_index(dir)];
}

extern std::array<std::array<Bitboard, 64>, 64> line_table;
extern std::array<std::array<Bitboard, 64>, 64> between_table;
extern std::array<std::array<Bitboard, 64>, 64> ray_pass_table;
// Initializes the line, between, and ray_pass tables.
void init_geometry_tables() noexcept;
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