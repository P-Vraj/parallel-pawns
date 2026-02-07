#pragma once
#include <span>
#include "types.h"

constexpr std::array<Bitboard, 64> getNonSlidingAttacks(std::span<const std::pair<int, int>> deltas) {
    std::array<Bitboard, 64> attacks{};

    for (size_t i = 0; i < 64; ++i) {
        Square sq = static_cast<Square>(i);
        Bitboard attacksBitboard = 0;
        for (const auto& [dr, df] : deltas) {
            File f = file(sq) + df;
            Rank r = rank(sq) + dr;
            if (isValid(f, r)) {
                attacksBitboard |= bitboard(make_square(f, r));
            }
        }
        attacks[to_underlying(sq)] = attacksBitboard;
    }

    return attacks;
}

constexpr std::array<Bitboard, 64> getKnightAttacks() {
    constexpr std::array<std::pair<int, int>, 8> knightMoves{{
        {+2, +1}, {+2, -1}, {+1, +2}, {+1, -2}, {-2, +1}, {-2, -1}, {-1, +2}, {-1, -2},
    }};
    return getNonSlidingAttacks(knightMoves);
}

constexpr std::array<Bitboard, 64> getKingAttacks() {
    constexpr std::array<std::pair<int, int>, 8> kingMoves{{
        {+1, 0}, {-1, 0}, {0, +1}, {0, -1}, {+1, +1}, {+1, -1}, {-1, +1}, {-1, -1},
    }};
    return getNonSlidingAttacks(kingMoves);
}

template <Color C>
constexpr std::array<Bitboard, 64> getPawnAttacks() {
    constexpr int forward = (C == Color::White) ? +1 : -1;
    constexpr std::array<std::pair<int, int>, 2> pawnMoves{{
        {forward, +1}, {forward, -1}}
    };
    return getNonSlidingAttacks(pawnMoves);
}

constexpr inline std::array<Bitboard, 64> kKnightAttacks = getKnightAttacks();
constexpr inline std::array<Bitboard, 64> kKingAttacks = getKingAttacks();
constexpr inline std::array<std::array<Bitboard, 64>, 2> kPawnAttacks = {
    getPawnAttacks<Color::White>(), getPawnAttacks<Color::Black>()
};