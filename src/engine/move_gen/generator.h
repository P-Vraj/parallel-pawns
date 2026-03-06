#pragma once
#include "../geometry.h"
#include "../position.h"
#include "../util.h"
#include "attacks.h"

struct MoveList;
struct PinsInfo;

// Checks if a piece of the given type can move in the given direction
constexpr inline bool is_slider_for_direction(PieceType pt, Direction dir) noexcept {
    switch (dir) {
        case Direction::North:
        case Direction::South:
        case Direction::East:
        case Direction::West:
            return pt == PieceType::Rook || pt == PieceType::Queen;

        case Direction::NorthEast:
        case Direction::NorthWest:
        case Direction::SouthEast:
        case Direction::SouthWest:
            return pt == PieceType::Bishop || pt == PieceType::Queen;

        default:
            return false;
    }
}

// Returns a bitboard of pieces giving check to the king of the given color.
Bitboard checkers(const Position& pos, Color us) noexcept;
// Checks if the given square is attacked by any piece of the given color, given the occupancy bitboard.
bool is_square_attacked(const Position& pos, Square sq, Color by, Bitboard occupied) noexcept;
// Checks if the given square is attacked by any piece of the given color, using the position's occupancy.
bool is_square_attacked(const Position& pos, Square sq, Color by) noexcept;
// Checks if any square in the given bitboard is attacked by any piece of the given color.
bool is_any_square_attacked(const Position& pos, Bitboard b, Color by) noexcept;
// Computes pinned pieces and their corresponding pin rays for the given position and side to move.
PinsInfo compute_pins(const Position& pos, Color us) noexcept;
// Generates all legal moves for the given position and appends them to the move list.
void generate_moves(const Position& pos, MoveList& moveList) noexcept;

// List of moves, with a max size of 256 (max moves in any given position is known to be 218)
struct MoveList {
    MoveList() noexcept = default;
    explicit MoveList(const Position& pos) noexcept { generate_moves(pos, *this); }
    constexpr void clear() noexcept { size_ = 0; }
    constexpr uint8_t size() const noexcept { return size_; }
    constexpr void push_back(Move m) noexcept { data_[size_++] = m; }
    constexpr Move operator[](int i) const noexcept { return data_[i]; }
    constexpr Move& operator[](int i) noexcept { return data_[i]; }
    constexpr Move* begin() noexcept { return data_.data(); }
    constexpr const Move* begin() const noexcept { return data_.data(); }
    constexpr Move* end() noexcept { return data_.data() + size_; }
    constexpr const Move* end() const noexcept { return data_.data() + size_; }

private:
    std::array<Move, 256> data_{};
    uint8_t size_ = 0;
};

struct PinsInfo {
    std::array<Bitboard, 64> pinRay{};  // Allowed destinations if pinned; ~0 if not pinned
    Bitboard pinned = 0;
};