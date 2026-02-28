#pragma once
#include <string>

#include "types.h"

struct Position {
    template <Color C, PieceType PT>
    constexpr Bitboard get() const {
        return pieces_[to_underlying(C)][to_underlying(PT) - 1];
    }
    constexpr Bitboard get(Color c, PieceType pt) const { return pieces_[to_underlying(c)][to_underlying(pt) - 1]; }
    constexpr Piece pieceOn(Square sq) const { return pieceMap_[to_underlying(sq)]; }
    template <Color C>
    constexpr Bitboard occupancy() const {
        return colorOccupied_[to_underlying(C)];
    }
    constexpr Bitboard occupancy() const { return occupied_; }
    constexpr Square kingSquare(Color c) const noexcept { return kingSquare_[to_underlying(c)]; }
    constexpr Color sideToMove() const noexcept { return sideToMove_; }
    constexpr Square epSquare() const noexcept { return enPassantSquare_; }
    constexpr CastlingRights castlingRights() const noexcept { return castlingRights_; }
    constexpr Key hash() const noexcept { return hash_; }
    constexpr uint16_t fullmoveNumber() const noexcept { return fullmoveNumber_; }
    constexpr uint8_t halfmoveClock() const noexcept { return halfmoveClock_; }

    static Position fromFEN(std::string_view fen);
    std::string toFEN() const;

private:
    std::array<std::array<Bitboard, to_underlying(PieceType::Count) - 1>, to_underlying(Color::Count)> pieces_;
    std::array<Piece, 64> pieceMap_;
    std::array<Bitboard, to_underlying(Color::Count)> colorOccupied_;
    Bitboard occupied_;
    std::array<Square, to_underlying(Color::Count)> kingSquare_;
    Color sideToMove_;
    CastlingRights castlingRights_;
    uint16_t fullmoveNumber_;
    uint8_t halfmoveClock_;
    Square enPassantSquare_;
    Key hash_;

    void parsePieceMap_(std::string_view placement) noexcept;
    void parseCastlingRights_(std::string_view castling) noexcept;
    // Fills the piece bitboards and occupancy bitboards based on the pieceMap_.
    void fillBitboards_() noexcept;
};

static_assert(sizeof(Position) == 200);