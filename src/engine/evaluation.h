#pragma once

#include "move_gen/attacks.h"
#include "position.h"
#include "types.h"
#include "util.h"

inline constexpr std::array<int16_t, to_underlying(PieceType::Count)> kPieceValues = {
    0,      // None
    100,    // Pawn
    300,    // Knight
    330,    // Bishop
    500,    // Rook
    900,    // Queen
    20000,  // King
};

inline constexpr std::array<int, to_underlying(PieceType::Count)> kMobilityWeights = {
    0,  // None
    0,  // Pawn
    4,  // Knight
    4,  // Bishop
    2,  // Rook
    1,  // Queen
    0,  // King
};

// clang-format off
inline constexpr std::array<int, 64> kPawnPST = {
    +0,   +0,   +0,  +0,  +0,  +0,  +0,  +0,
    +50, +50,  +50, +50, +50, +50, +50, +50,
    +10, +10,  +20, +30, +30, +20, +10, +10,
    +5,   +5,  +10, +25, +25, +10,  +5,  +5,
    +0,   +0,   +0, +20, +20,  +0,  +0,  +0,
    +5,   -5,  -10,  +0,  +0, -10,  -5,  +5,
    +5,  +10,  +10, -20, -20, +10, +10,  +5,
    +0,   +0,   +0,  +0,  +0,  +0,  +0,  +0,
};

inline constexpr std::array<int, 64> kKnightPST = {
    -50, -40,  -30, -30, -30, -30,  -40, -50,
    -40, -20,   +0,  +0,  +0,  +0,  -20, -40,
    -30,  +0,  +10, +15, +15, +10,   +0, -30,
    -30,  +5,  +15, +20, +20, +15,   +5, -30,
    -30,  +0,  +15, +20, +20, +15,   +0, -30,
    -30,  +5,  +10, +15, +15, +10,   +5, -30,
    -40, -20,   +0,  +5,  +5,  +0,  -20, -40,
    -50, -40,  -30, -30, -30, -30,  -40, -50,
};

inline constexpr std::array<int, 64> kBishopPST = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,  +0,  +0,  +0,  +0,  +0,  +0, -10,
    -10,  +0,  +5, +10, +10,  +5,  +0, -10,
    -10,  +5,  +5, +10, +10,  +5,  +5, -10,
    -10,  +0, +10, +10, +10, +10,  +0, -10,
    -10, +10, +10, +10, +10, +10, +10, -10,
    -10,  +5,  +0,  +0,  +0,  +0,  +5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

inline constexpr std::array<int, 64> kRookPST = {
    +0,  +0,  +0,  +5,  +5,  +0,  +0, +0,
    -5,  +0,  +0,  +0,  +0,  +0,  +0, -5,
    -5,  +0,  +0,  +0,  +0,  +0,  +0, -5,
    -5,  +0,  +0,  +0,  +0,  +0,  +0, -5,
    -5,  +0,  +0,  +0,  +0,  +0,  +0, -5,
    -5,  +0,  +0,  +0,  +0,  +0,  +0, -5,
    +5, +10, +10, +10, +10, +10, +10, +5,
    +0,  +0,  +0,  +0,  +0,  +0,  +0, +0,
};

inline constexpr std::array<int, 64> kQueenPST = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,  +0,  +0,  +0,  +0,  +0,  +0, -10,
    -10,  +0,  +5,  +5,  +5,  +5,  +0, -10,
    -5,   +0,  +0,  +5,  +5,  +5,  +5,  -5,
    +0,   +0,  +5,  +5,  +5,  +5,  +0,  -5,
    -10,  +5,  +5,  +5,  +5,  +5,  +0, -10,
    -10,  +0,  +5,  +0,  +0,  +0,  +0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20,
};

inline constexpr std::array<int, 64> kKingPST = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    +20, +20,  +0,  +0,  +0,  +0, +20, +20,
    +20, +30, +10,  +0,  +0, +10, +30, +20,
};
// clang-format on

inline constexpr int kBishopPairBonus = 30;

constexpr Square relative_square(Color c, Square sq) noexcept {
    return (c == Color::White) ? sq : static_cast<Square>(to_underlying(sq) ^ 56);
}

inline int piece_square_bonus(PieceType pt, Color c, Square sq) noexcept {
    const auto relativeSq = to_underlying(relative_square(c, sq));
    switch (pt) {
        case PieceType::Pawn:
            return kPawnPST[relativeSq];
        case PieceType::Knight:
            return kKnightPST[relativeSq];
        case PieceType::Bishop:
            return kBishopPST[relativeSq];
        case PieceType::Rook:
            return kRookPST[relativeSq];
        case PieceType::Queen:
            return kQueenPST[relativeSq];
        case PieceType::King:
            return kKingPST[relativeSq];
        case PieceType::None:
        default:
            return 0;
    }
}

inline Eval material_diff(const Position& pos) noexcept {
    int score = 0;

    for (PieceType pt = PieceType::Pawn; pt <= PieceType::Queen; ++pt) {
        const int value = kPieceValues[to_underlying(pt)];

        score += value * bit_count(pos.get(Color::White, pt));
        score -= value * bit_count(pos.get(Color::Black, pt));
    }

    return static_cast<Eval>(score);
}

inline Eval piece_square_diff(const Position& pos) noexcept {
    int score = 0;

    for (const Color c : {Color::White, Color::Black}) {
        const int sign = (c == Color::White) ? 1 : -1;

        for (PieceType pt = PieceType::Pawn; pt <= PieceType::King; ++pt) {
            Bitboard pieces = pos.get(c, pt);
            while (pieces) {
                const auto sq = static_cast<Square>(pop_lsb(pieces));
                score += sign * piece_square_bonus(pt, c, sq);
            }
        }
    }

    return static_cast<Eval>(score);
}

inline Eval bishop_pair_diff(const Position& pos) noexcept {
    int score = 0;
    if (bit_count(pos.get(Color::White, PieceType::Bishop)) >= 2)
        score += kBishopPairBonus;
    if (bit_count(pos.get(Color::Black, PieceType::Bishop)) >= 2)
        score -= kBishopPairBonus;

    return static_cast<Eval>(score);
}

inline Eval mobility_diff(const Position& pos) noexcept {
    int score = 0;
    const Bitboard occ = pos.occupancy();

    for (const Color c : {Color::White, Color::Black}) {
        const int sign = (c == Color::White) ? 1 : -1;
        const Bitboard usOcc = pos.occupancy(c);

        for (const PieceType pt : {PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen}) {
            Bitboard pieces = pos.get(c, pt);
            while (pieces) {
                const auto sq = static_cast<Square>(pop_lsb(pieces));
                const Bitboard attacks = attacks::piece_attacks(pt, sq, occ) & ~usOcc;
                score += sign * (bit_count(attacks) * kMobilityWeights[to_underlying(pt)]);
            }
        }
    }

    return static_cast<Eval>(score);
}

inline Eval evaluation(const Position& pos) noexcept {
    int score = 0;

    score += material_diff(pos);
    score += piece_square_diff(pos);
    score += bishop_pair_diff(pos);
    score += mobility_diff(pos);

    return static_cast<Eval>(score);
};
