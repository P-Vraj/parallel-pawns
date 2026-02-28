#pragma once
#include "../position.h"
#include "../util.h"
#include "attacks.h"
#include "geometry.h"

// List of moves, with a max size of 256 (max moves in any given position is known to be 218)
struct MoveList {
    constexpr void clear() noexcept { size_ = 0; }
    constexpr uint8_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr void push_back(Move m) noexcept { data_[size_++] = m; }
    constexpr Move back() const noexcept { return data_[size_ - 1]; }
    void pop_back() noexcept { --size_; }
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
    Bitboard pinned = 0;
    std::array<Bitboard, 64> pinRay{};  // Allowed destinations if pinned; ~0 if not pinned
};

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
// Checks if the given square is attacked by any piece of the given color.
bool is_square_attacked(const Position& pos, Square sq, Color by) noexcept;
// Computes pinned pieces and their corresponding pin rays for the given position and side to move.
PinsInfo compute_pins(const Position& pos, Color us) noexcept;
// Generates all legal moves for the given position and appends them to the move list.
void generate_moves(const Position& pos, MoveList& moveList) noexcept;
// Generates legal king moves and appends them to the move list.
void king_moves(const Position& pos, MoveList& moveList) noexcept;