#include "geometry.h"

namespace {

inline constexpr Bitboard kFileA = 0x0101010101010101ULL;
inline constexpr Bitboard kFileH = 0x8080808080808080ULL;
inline constexpr Bitboard kRank1 = 0x00000000000000FFULL;
inline constexpr Bitboard kRank8 = 0xFF00000000000000ULL;

// Rays are like directions, but 0-indexed (used to index Direction)
enum class Ray : uint8_t {
    North,
    South,
    East,
    West,
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,

    Count = 8
};

};  // namespace

constexpr inline bool can_step(Square sq, Direction dir) noexcept {
    const Bitboard bb = bitboard(sq);
    switch (dir) {
        case Direction::North:
            return (bb & kRank8) == 0;
        case Direction::South:
            return (bb & kRank1) == 0;
        case Direction::East:
            return (bb & kFileH) == 0;
        case Direction::West:
            return (bb & kFileA) == 0;
        case Direction::NorthEast:
            return (bb & (kRank8 | kFileH)) == 0;
        case Direction::NorthWest:
            return (bb & (kRank8 | kFileA)) == 0;
        case Direction::SouthEast:
            return (bb & (kRank1 | kFileH)) == 0;
        case Direction::SouthWest:
            return (bb & (kRank1 | kFileA)) == 0;
        default:
            return false;
    }
}

constexpr inline bool aligned_dir(Square from, Square to, Direction& out) noexcept {
    const int df = static_cast<int>(file(from)) - static_cast<int>(file(to));
    const int dr = static_cast<int>(rank(from)) - static_cast<int>(rank(to));

    if (df == 0 && dr > 0) {
        out = Direction::North;
        return true;
    }
    if (df == 0 && dr < 0) {
        out = Direction::South;
        return true;
    }
    if (dr == 0 && df > 0) {
        out = Direction::East;
        return true;
    }
    if (dr == 0 && df < 0) {
        out = Direction::West;
        return true;
    }

    if (df == dr && df > 0) {
        out = Direction::NorthEast;
        return true;
    }
    if (df == dr && df < 0) {
        out = Direction::SouthWest;
        return true;
    }
    if (df == -dr && df > 0) {
        out = Direction::SouthEast;
        return true;
    }
    if (df == -dr && df < 0) {
        out = Direction::NorthWest;
        return true;
    }

    return false;
}

constexpr inline Bitboard between(Square a, Square b) noexcept {
    Direction dir;
    if (a == b || !aligned_dir(a, b, dir))
        return 0;

    Bitboard bb = 0;
    Square s = a;
    while (can_step(s, dir)) {
        s += dir;
        if (s == b)
            break;
        bb |= bitboard(s);
    }

    return bb;
}
