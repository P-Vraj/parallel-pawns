#pragma once

#include "types.h"

constexpr Eval kEvalInf = 32000;
constexpr Eval kEvalNegInf = -kEvalInf;

constexpr Eval kMateScore = 30000;
constexpr Eval kMateThreshold = 29000;

constexpr Eval kDrawScore = 0;

constexpr int kMaxPly = 128;

constexpr bool isMateScore(Eval score) noexcept {
    return score > kMateThreshold || score < -kMateThreshold;
}
constexpr bool isWinningMate(Eval score) noexcept {
    return score > kMateThreshold;
}
constexpr bool isLosingMate(Eval score) noexcept {
    return score < -kMateThreshold;
}
constexpr Eval matedScore(int ply) noexcept {
    return -kMateScore + ply;
}
constexpr Eval encodeMateScore(Eval score, int ply) noexcept {
    if (isWinningMate(score))
        return static_cast<Eval>(score + ply);
    if (isLosingMate(score))
        return static_cast<Eval>(score - ply);
    return score;
}
constexpr Eval decodeMateScore(Eval score, int ply) noexcept {
    if (isWinningMate(score))
        return static_cast<Eval>(score - ply);
    if (isLosingMate(score))
        return static_cast<Eval>(score + ply);
    return score;
}