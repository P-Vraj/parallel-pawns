#pragma once
#include "attacks_leapers.h"
#include "attacks_sliders.h"

namespace attacks {

// Initializes the attack tables for rooks and bishops. Must be called at startup.
inline void init_attack_tables() noexcept {
    init_rook_attacks_table();
    init_bishop_attacks_table();
}

inline Bitboard rook_attacks(Square sq, Bitboard occ) noexcept {
    const Magic& magic = kRookMagics[to_underlying(sq)];
    const size_t index = magic_index(magic, occ);
    return kRookAttacksTable[(to_underlying(sq) * kRookStride) + index];
}

inline Bitboard bishop_attacks(Square sq, Bitboard occ) noexcept {
    const Magic& magic = kBishopMagics[to_underlying(sq)];
    const size_t index = magic_index(magic, occ);
    return kBishopAttacksTable[(to_underlying(sq) * kBishopStride) + index];
}

inline Bitboard queen_attacks(Square sq, Bitboard occ) noexcept {
    return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}

inline Bitboard knight_attacks(Square sq) noexcept {
    return kKnightAttacksTable[to_underlying(sq)];
}

inline Bitboard king_attacks(Square sq) noexcept {
    return kKingAttacksTable[to_underlying(sq)];
}

inline Bitboard pawn_attacks(Color c, Square sq) noexcept {
    return kPawnAttacksTable[to_underlying(c)][to_underlying(sq)];
}

// Returns a bitboard of squares attacked by a piece of the given type for the square and (optional) occupancy.
template <PieceType PT>
    requires(PT == PieceType::Knight || PT == PieceType::Bishop || PT == PieceType::Rook || PT == PieceType::Queen)
constexpr Bitboard piece_attacks(Square sq, Bitboard occ) noexcept {
    if constexpr (PT == PieceType::Knight)
        return knight_attacks(sq);
    if constexpr (PT == PieceType::Bishop)
        return bishop_attacks(sq, occ);
    if constexpr (PT == PieceType::Rook)
        return rook_attacks(sq, occ);
    if constexpr (PT == PieceType::Queen)
        return queen_attacks(sq, occ);
    return Bitboard{ 0 };
}

}  // namespace attacks