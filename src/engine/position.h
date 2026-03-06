#pragma once
#include <string>

#include "types.h"

struct UndoInfo {
    Piece captured = Piece::None;
    CastlingRights castlingRights{};
    Square enPassantSquare = Square::None;
    uint8_t halfmoveClock = 0;
    Key hash{};
};
static_assert(sizeof(UndoInfo) == 16);

struct Position {
    // Returns the bitboard of pieces of the given color and piece type.
    template <Color C, PieceType PT>
    constexpr Bitboard get() const {
        return pieces_[to_underlying(C)][to_underlying(PT) - 1];
    }
    // Returns the bitboard of pieces of the given color and piece type.
    constexpr Bitboard get(Color c, PieceType pt) const { return pieces_[to_underlying(c)][to_underlying(pt) - 1]; }
    // Returns the piece on the given square, or Piece::None if the square is empty.
    constexpr Piece pieceOn(Square sq) const { return pieceMap_[to_underlying(sq)]; }
    // Returns the bitboard of pieces of the given color
    constexpr Bitboard occupancy(Color c) const { return colorOccupied_[to_underlying(c)]; }
    // Returns the bitboard of all occupied squares.
    constexpr Bitboard occupancy() const { return occupied_; }
    constexpr Square kingSquare(Color c) const noexcept { return kingSquare_[to_underlying(c)]; }
    constexpr Color sideToMove() const noexcept { return sideToMove_; }
    constexpr Square epSquare() const noexcept { return enPassantSquare_; }
    constexpr CastlingRights castlingRights() const noexcept { return castlingRights_; }
    constexpr Key hash() const noexcept { return hash_; }
    constexpr uint16_t fullmoveNumber() const noexcept { return fullmoveNumber_; }
    constexpr uint8_t halfmoveClock() const noexcept { return halfmoveClock_; }

    // Creates a Position from a FEN string. Assumes the FEN is valid and well-formed.
    static Position fromFEN(std::string_view fen);
    // Converts the Position back to a FEN string.
    std::string toFEN() const;
    // Makes the given move on the position, updating the position and filling in the undo information.
    void makeMove(Move m, UndoInfo& undo) noexcept;
    // Undoes the given move using the provided undo information, restoring the position to its previous state.
    void undoMove(Move m, const UndoInfo& undo) noexcept;
    // Computes the Zobrist hash of the position. Only needed at initialization, as the hash is updated incrementally.
    Key computeHash() const noexcept;

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
    Square enPassantSquare_; // Candidate en passant square
    Key hash_;

    void parsePieceMap_(std::string_view placement) noexcept;
    void parseCastlingRights_(std::string_view castling) noexcept;
    // Fills the piece bitboards and occupancy bitboards based on the pieceMap_.
    void fillBitboards_() noexcept;
    void removePiece_(Square sq) noexcept;
    void putPiece_(Square sq, Piece piece) noexcept;
    void movePiece_(Square from, Square to) noexcept;
};
static_assert(sizeof(Position) == 200);