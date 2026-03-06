#pragma once

#include "position.h"
#include "types.h"
#include "util.h"

inline constexpr std::array<int, to_underlying(PieceType::Count)> kPieceValues = {
    0,      // None
    100,    // Pawn
    320,    // Knight
    330,    // Bishop
    500,    // Rook
    900,    // Queen
    20000,  // King
};

inline Eval material_diff(const Position& pos) noexcept {
    Eval score = 0;

    for (PieceType pt = PieceType::Pawn; pt <= PieceType::Queen; ++pt) {
        const int value = kPieceValues[to_underlying(pt)];

        score += value * bit_count(pos.get(Color::White, pt));
        score -= value * bit_count(pos.get(Color::Black, pt));
    }

    return score;
}

inline Eval evaluation(const Position& pos) noexcept {
    Eval score = 0;

    score += material_diff(pos);

    return score;
};