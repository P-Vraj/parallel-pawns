#pragma once
#include "types.h"

struct Position {
    template <Color C, PieceType PT>
    constexpr Bitboard get() const {
        static_assert(C < Color::Count);
        static_assert(PT >= PieceType::Pawn && PT <= PieceType::King);
        return pieces_[to_underlying(C)][to_underlying(PT) - 1];
    }

    constexpr Bitboard get(Color c, PieceType pt) const { return pieces_[to_underlying(c)][to_underlying(pt) - 1]; }

    constexpr Piece pieceOn(Square sq) const { return pieceMap_[to_underlying(sq)]; }

    template <Color C>
    constexpr Bitboard occupancy() const {
        static_assert(C < Color::Count);
        return colorOccupied_[to_underlying(C)];
    }

    constexpr Bitboard occupancy() const { return occupied_; }

    // Recomputes occupancy bitboards (used during changes not reachable through move/unmove)
    void recomputeOccupancy() {
        for (size_t c = 0; c < to_underlying(Color::Count); ++c) {
            Bitboard occ = 0;
            for (size_t pt = to_underlying(PieceType::Pawn); pt < to_underlying(PieceType::Count); ++pt) {
                occ |= get(Color(c), PieceType(pt));
            }
            colorOccupied_[c] = occ;
        }
        occupied_ = occupancy<Color::White>() | occupancy<Color::Black>();
    }

private:
    std::array<std::array<Bitboard, to_underlying(PieceType::Count) - 1>, to_underlying(Color::Count)> pieces_;
    std::array<Piece, 64> pieceMap_;
    std::array<Bitboard, to_underlying(Color::Count)> colorOccupied_;
    Bitboard occupied_;
    std::array<Square, to_underlying(Color::Count)> kingSquare_;
    Color sideToMove_;
    CastlingRights castlingRights_;
    Square enPassantSquare_;
    uint64_t hash_;
};

static_assert(sizeof(Position) == 200);  // actual size is 197