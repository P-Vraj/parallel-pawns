#include "geometry.h"

#include "move_gen/attacks.h"

namespace {

inline constexpr Bitboard kFileA = 0x0101010101010101ULL;
inline constexpr Bitboard kFileH = 0x8080808080808080ULL;
inline constexpr Bitboard kRank1 = 0x00000000000000FFULL;
inline constexpr Bitboard kRank8 = 0xFF00000000000000ULL;

};  // namespace

namespace geom {

inline std::array<std::array<Bitboard, 64>, 64> line_table{};
inline std::array<std::array<Bitboard, 64>, 64> between_table{};
inline std::array<std::array<Bitboard, 64>, 64> ray_pass_table{};

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

void init_geometry_tables() noexcept {
    using namespace attacks;
    for (size_t i = 0; i < 64; ++i) {
        const auto from = static_cast<Square>(i);
        const Bitboard fromBB = bitboard(from);

        const Bitboard orthogonalFrom = rook_attacks(from, 0);
        const Bitboard diagonalFrom = bishop_attacks(from, 0);

        for (size_t j = 0; j < 64; ++j) {
            const auto to = static_cast<Square>(j);
            const Bitboard toBB = bitboard(to);

            Bitboard line = 0;
            Bitboard between = 0;
            Bitboard rayPass = 0;
            if (orthogonalFrom & toBB) {
                const Bitboard orthogonalTo = rook_attacks(to, 0);

                line = (orthogonalFrom & orthogonalTo) | fromBB | toBB;
                between = rook_attacks(from, toBB) & rook_attacks(to, fromBB);
                rayPass = orthogonalFrom & (rook_attacks(to, fromBB) | toBB);
            }
            else if (diagonalFrom & toBB) {
                const Bitboard diagonalTo = bishop_attacks(to, 0);

                line = (diagonalFrom & diagonalTo) | fromBB | toBB;
                between = bishop_attacks(from, toBB) & bishop_attacks(to, fromBB);
                rayPass = diagonalFrom & (bishop_attacks(to, fromBB) | toBB);
            }

            line_table[i][j] = line;
            between_table[i][j] = between;
            ray_pass_table[i][j] = rayPass;
        }
    }
}

inline Bitboard between(Square a, Square b) noexcept {
    return between_table[to_underlying(a)][to_underlying(b)];
}

inline Bitboard between_or_to(Square a, Square b) noexcept {
    return between_table[to_underlying(a)][to_underlying(b)] | bitboard(b);
}

inline Bitboard line(Square a, Square b) noexcept {
    return line_table[to_underlying(a)][to_underlying(b)];
}

inline Bitboard ray_pass(Square a, Square b) noexcept {
    return ray_pass_table[to_underlying(a)][to_underlying(b)];
}

};  // namespace geom