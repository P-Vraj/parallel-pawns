#pragma once

#include "types.h"

constexpr Eval kEvalInf = 32000;
constexpr Eval kEvalNegInf = -kEvalInf;

constexpr Eval kMateScore = 30000;
constexpr Eval kMateThreshold = 29000;

constexpr Eval kDrawScore = 0;

constexpr int kMaxPly = 128;

constexpr bool is_mate_score(Eval score) noexcept {
    return score > kMateThreshold || score < -kMateThreshold;
}

constexpr bool is_winning_mate(Eval score) noexcept {
    return score > kMateThreshold;
}

constexpr bool is_losing_mate(Eval score) noexcept {
    return score < -kMateThreshold;
}

constexpr Eval mated_score(int ply) noexcept {
    return -kMateScore + ply;
}

// Encodes a mate score to be relative to the current ply, to differentiate mate scores at different depths
constexpr Eval encode_mate_score(Eval score, int ply) noexcept {
    if (is_winning_mate(score))
        return score + ply;
    if (is_losing_mate(score))
        return score - ply;
    return score;
}

// Decodes a mate score from being relative to the current ply back to an absolute score
constexpr Eval decode_mate_score(Eval score, int ply) noexcept {
    if (is_winning_mate(score))
        return score - ply;
    if (is_losing_mate(score))
        return score + ply;
    return score;
}

// Packs an Eval score into a TTScore to efficiently store in the transposition table
constexpr TTScore pack_TTScore(Eval score) noexcept {
    if (score < std::numeric_limits<TTScore>::min())
        return std::numeric_limits<TTScore>::min();
    if (score > std::numeric_limits<TTScore>::max())
        return std::numeric_limits<TTScore>::max();
    return static_cast<TTScore>(score);
}

// Unpacks a TTScore back into an Eval score
constexpr Eval unpack_TTScore(TTScore score) noexcept {
    return static_cast<Eval>(score);
}
