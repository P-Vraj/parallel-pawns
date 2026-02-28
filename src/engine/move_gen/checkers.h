#pragma once
#include "attacks.h"
#include "geometry.h"
#include "position.h"
#include "util.h"

inline Bitboard checkers(const Position& pos, Color us) noexcept {
    using namespace attacks;
    const Color opp = ~us;
    const Square kingSq = pos.kingSquare(us);
    const Bitboard occupied = pos.occupancy();

    Bitboard checkers = 0;
    checkers |= pawn_attacks(us, kingSq) & pos.get(opp, PieceType::Pawn);
    checkers |= knight_attacks(kingSq) & pos.get(opp, PieceType::Knight);
    checkers |= bishop_attacks(kingSq, occupied) & pos.get(opp, PieceType::Bishop);
    checkers |= rook_attacks(kingSq, occupied) & pos.get(opp, PieceType::Rook);
    checkers |= queen_attacks(kingSq, occupied) & pos.get(opp, PieceType::Queen);
    return checkers;
}

inline bool is_square_attacked(const Position& pos, Square sq, Color by) noexcept {
    using namespace attacks;
    const Color opp = ~by;
    const Bitboard occupied = pos.occupancy();

    if (pawn_attacks(opp, sq) & pos.get(by, PieceType::Pawn))
        return true;
    if (knight_attacks(sq) & pos.get(by, PieceType::Knight))
        return true;
    if (bishop_attacks(sq, occupied) & pos.get(by, PieceType::Bishop))
        return true;
    if (rook_attacks(sq, occupied) & pos.get(by, PieceType::Rook))
        return true;
    if (queen_attacks(sq, occupied) & pos.get(by, PieceType::Queen))
        return true;
    if (king_attacks(sq) & pos.get(by, PieceType::King))
        return true;
    return false;
}

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

struct PinsInfo {
    Bitboard pinned = 0;
    std::array<Bitboard, 64> pinRay;  // allowed destinations if pinned; ~0 if not pinned
};

PinsInfo compute_pins(const Position& pos, Color us) noexcept {
    PinsInfo pins;
    pins.pinRay.fill(~Bitboard{ 0 });

    const Color them = ~us;
    const Square kingSq = pos.kingSquare(us);

    for (Direction dir : kDirs) {
        Square s = kingSq;
        Square candidate = Square::None;
        bool foundFriendly = false;

        while (can_step(s, dir)) {
            s += dir;
            Piece p = pos.pieceOn(s);

            if (is_empty(p))
                continue;

            if (color(p) == us) {
                if (!foundFriendly) {
                    foundFriendly = true;
                    candidate = s;
                    continue;
                }
                else {
                    break;  // second friendly piece
                }
            }
            else {
                if (foundFriendly && is_slider_for_direction(piece_type(p), dir)) {
                    pins.pinned |= bitboard(candidate);

                    // build ray from king to pinner
                    Bitboard ray = 0;
                    Square t = kingSq;
                    while (can_step(t, dir)) {
                        t += dir;
                        ray |= bitboard(t);
                        if (t == s)
                            break;
                    }

                    pins.pinRay[to_underlying(candidate)] = ray;
                }
                break;
            }
        }
    }

    return pins;
}