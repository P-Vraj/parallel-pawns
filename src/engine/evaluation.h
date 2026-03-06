#pragma once

#include "position.h"
#include "util.h"
#include "types.h"


void init_evaluation(Position& pos) noexcept{
    int16_t score = 0;

    for (PieceType pt = PieceType::Pawn; pt <= PieceType::Queen; ++pt) {
        const int value = piece_values[to_underlying(pt)];

        score += value * bit_count(pos.get(Color::White, pt));
        score -= value * bit_count(pos.get(Color::Black, pt));
    }
    pos.setEvaluation(score);
};