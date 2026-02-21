// Implements "magic bitboards" to speed up move generation by precaching results
// Learn more here: https://en.wikipedia.org/wiki/Bitboard#Magic_bitboards

#pragma once
#include <array>

#include "magics_generated.h"

const size_t kRookStride = 4096;
const size_t kBishopStride = 512;
inline std::array<Bitboard, 64 * kRookStride> kRookAttacksTable;
inline std::array<Bitboard, 64 * kBishopStride> kBishopAttacksTable;

constexpr inline size_t magic_index(const Magic& magic, Bitboard blockers) noexcept {
    return static_cast<size_t>(((blockers & magic.mask) * magic.magic) >> magic.shift);
}

constexpr inline Bitboard rook_mask(Square sq) noexcept {
    Bitboard mask = 0;
    int curFile = to_underlying(file(sq));
    int curRank = to_underlying(rank(sq));

    // Exclude the edges as they have no impact on the sliding attacks
    for (int f = curFile + 1; f <= 6; ++f) {
        mask |= bitboard(make_square(static_cast<File>(f), static_cast<Rank>(curRank)));
    }
    for (int f = curFile - 1; f >= 1; --f) {
        mask |= bitboard(make_square(static_cast<File>(f), static_cast<Rank>(curRank)));
    }
    for (int r = curRank + 1; r <= 6; ++r) {
        mask |= bitboard(make_square(static_cast<File>(curFile), static_cast<Rank>(r)));
    }
    for (int r = curRank - 1; r >= 1; --r) {
        mask |= bitboard(make_square(static_cast<File>(curFile), static_cast<Rank>(r)));
    }

    return mask;
}

constexpr inline Bitboard bishop_mask(Square sq) noexcept {
    Bitboard mask = 0;
    int curFile = to_underlying(file(sq));
    int curRank = to_underlying(rank(sq));

    // Exclude the edges as they have no impact on the sliding attacks
    for (int f = curFile + 1, r = curRank + 1; f <= 6 && r <= 6; ++f, ++r) {
        mask |= bitboard(make_square(static_cast<File>(f), static_cast<Rank>(r)));
    }
    for (int f = curFile - 1, r = curRank + 1; f >= 1 && r <= 6; --f, ++r) {
        mask |= bitboard(make_square(static_cast<File>(f), static_cast<Rank>(r)));
    }
    for (int f = curFile + 1, r = curRank - 1; f <= 6 && r >= 1; ++f, --r) {
        mask |= bitboard(make_square(static_cast<File>(f), static_cast<Rank>(r)));
    }
    for (int f = curFile - 1, r = curRank - 1; f >= 1 && r >= 1; --f, --r) {
        mask |= bitboard(make_square(static_cast<File>(f), static_cast<Rank>(r)));
    }

    return mask;
}

constexpr inline Bitboard rook_attacks_ray(Square sq, Bitboard occupancy) noexcept {
    Bitboard attacks = 0;
    int curFile = to_underlying(file(sq));
    int curRank = to_underlying(rank(sq));

    // North
    for (int r = curRank + 1; r <= 7; ++r) {
        Square to = make_square(static_cast<File>(curFile), static_cast<Rank>(r));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }
    // South
    for (int r = curRank - 1; r >= 0; --r) {
        Square to = make_square(static_cast<File>(curFile), static_cast<Rank>(r));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }
    // East
    for (int f = curFile + 1; f <= 7; ++f) {
        Square to = make_square(static_cast<File>(f), static_cast<Rank>(curRank));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }
    // West
    for (int f = curFile - 1; f >= 0; --f) {
        Square to = make_square(static_cast<File>(f), static_cast<Rank>(curRank));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }

    return attacks;
}

constexpr inline Bitboard bishop_attacks_ray(Square sq, Bitboard occupancy) noexcept {
    Bitboard attacks = 0;
    int curFile = to_underlying(file(sq));
    int curRank = to_underlying(rank(sq));

    // Northeast
    for (int f = curFile + 1, r = curRank + 1; f <= 7 && r <= 7; ++f, ++r) {
        Square to = make_square(static_cast<File>(f), static_cast<Rank>(r));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }
    // Southeast
    for (int f = curFile + 1, r = curRank - 1; f <= 7 && r >= 0; ++f, --r) {
        Square to = make_square(static_cast<File>(f), static_cast<Rank>(r));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }
    // Southwest
    for (int f = curFile - 1, r = curRank - 1; f >= 0 && r >= 0; --f, --r) {
        Square to = make_square(static_cast<File>(f), static_cast<Rank>(r));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }
    // Northwest
    for (int f = curFile - 1, r = curRank + 1; f >= 0 && r <= 7; --f, ++r) {
        Square to = make_square(static_cast<File>(f), static_cast<Rank>(r));
        attacks |= bitboard(to);
        if (occupancy & bitboard(to))
            break;
    }

    return attacks;
}

constexpr inline Bitboard index_to_occupancy(Bitboard mask, size_t index) noexcept {
    Bitboard occ = 0;
    while (mask) {
        Bitboard lsb = mask & (~mask + 1);
        mask ^= lsb;
        if (index & 1ULL)
            occ |= lsb;
        index >>= 1ULL;
    }
    return occ;
}

constexpr inline void init_rook_attacks_table() noexcept {
    for (size_t i = 0; i < 64; ++i) {
        Square sq = static_cast<Square>(i);
        Magic magic = kRookMagics[i];
        size_t relevantBlockerCount = 1ULL << (64 - magic.shift);

        for (size_t j = 0; j < relevantBlockerCount; ++j) {
            Bitboard blockers = index_to_occupancy(magic.mask, j);
            const size_t idx = magic_index(magic, blockers);
            kRookAttacksTable[(i * kRookStride) + idx] = rook_attacks_ray(sq, blockers);
        }
    }
}

constexpr inline void init_bishop_attacks_table() noexcept {
    for (size_t i = 0; i < 64; ++i) {
        Square sq = static_cast<Square>(i);
        Magic magic = kBishopMagics[i];
        size_t relevantBlockerCount = 1ULL << (64 - magic.shift);

        for (size_t j = 0; j < relevantBlockerCount; ++j) {
            Bitboard blockers = index_to_occupancy(magic.mask, j);
            const size_t idx = magic_index(magic, blockers);
            kBishopAttacksTable[(i * kBishopStride) + idx] = bishop_attacks_ray(sq, blockers);
        }
    }
}
