#include "generator.h"

namespace {

struct Context {
    Color us{}, them{};
    Square ksq{};
    Bitboard occ{}, usOcc{}, themOcc{};
    Bitboard checkers{};
    int numCheckers{};
    PinsInfo pins{};
    Bitboard evasionMask{};
};

// Generates the evasion mask for a king in check by a single piece.
Bitboard evasion_mask_single(const Position& pos, Square kingSq, Bitboard checkers) noexcept {
    const auto checkerSq = static_cast<Square>(get_lsb(checkers));
    const Piece checkerPiece = pos.pieceOn(checkerSq);
    const PieceType pt = piece_type(checkerPiece);

    // Either the checker can be captured, or if it's a slider, it can be blocked.
    Bitboard mask = bitboard(checkerSq);
    if (pt == PieceType::Bishop || pt == PieceType::Rook || pt == PieceType::Queen)
        mask |= geom::between(kingSq, checkerSq);

    return mask;
}

}  // namespace

Bitboard checkers(const Position& pos, Color us) noexcept {
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

bool is_square_attacked(const Position& pos, Square sq, Color by) noexcept {
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

PinsInfo compute_pins(const Position& pos, Color us) noexcept {
    PinsInfo pins;
    pins.pinRay.fill(~Bitboard{ 0 });

    const Square kingSq = pos.kingSquare(us);

    auto scan_direction = [&](Direction dir) noexcept {
        Square candidate = Square::None;
        for (Square sq = geom::step(kingSq, dir); is_valid(sq); sq = geom::step(sq, dir)) {
            const Piece p = pos.pieceOn(sq);
            if (is_empty(p))
                continue;

            // Check if the piece is a friendly piece blocking the pin
            if (color(p) == us) {
                if (!is_valid(candidate)) {
                    candidate = sq;
                    continue;
                }
                return;
            }
            // If enemy piece found that slides in this direction, it's a pin
            if (is_valid(candidate) && is_slider_for_direction(piece_type(p), dir)) {
                pins.pinned |= bitboard(candidate);
                // Build ray from king to pinner
                pins.pinRay[to_underlying(candidate)] = geom::between_or_to(kingSq, sq);
            }
            return;
        }
    };

    for (const Direction dir : geom::kDirections) {
        scan_direction(dir);
    }

    return pins;
}

void generate_moves(const Position& pos, MoveList& moveList) noexcept {}

void king_moves(const Position& pos, MoveList& moveList) noexcept {}