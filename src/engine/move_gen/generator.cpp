#include "generator.h"

namespace {

constexpr bool includes_quiets(GenMode mode) noexcept {
    return mode == GenMode::Legal || mode == GenMode::Evasions;
}

void push_simple_moves(Square from, Bitboard targets, Bitboard themOcc, MoveList& moveList) noexcept {
    while (targets) {
        const auto to = static_cast<Square>(pop_lsb(targets));
        const MoveType moveType = (themOcc & bitboard(to)) ? MoveType::Capture : MoveType::Normal;
        moveList.push_back(Move(from, to, moveType));
    }
}

void push_promotions(Square from, Square to, bool isCapture, MoveList& moveList) noexcept {
    if (!isCapture) {
        moveList.push_back(Move(from, to, MoveType::PromotionQueen));
        moveList.push_back(Move(from, to, MoveType::PromotionRook));
        moveList.push_back(Move(from, to, MoveType::PromotionBishop));
        moveList.push_back(Move(from, to, MoveType::PromotionKnight));
    }
    else {
        moveList.push_back(Move(from, to, MoveType::PromotionCaptureQueen));
        moveList.push_back(Move(from, to, MoveType::PromotionCaptureRook));
        moveList.push_back(Move(from, to, MoveType::PromotionCaptureBishop));
        moveList.push_back(Move(from, to, MoveType::PromotionCaptureKnight));
    }
}

// Generates the evasion mask for a king in check by a single piece.
Bitboard evasion_mask(const Position& pos, Square kingSq, Bitboard checkers) noexcept {
    const auto checkerSq = static_cast<Square>(get_lsb(checkers));
    const Piece checkerPiece = pos.pieceOn(checkerSq);
    const PieceType pt = piece_type(checkerPiece);

    // Either the checker can be captured, or if it's a slider, it can be blocked.
    Bitboard mask = bitboard(checkerSq);
    if (pt == PieceType::Bishop || pt == PieceType::Rook || pt == PieceType::Queen)
        mask |= geom::between(kingSq, checkerSq);

    return mask;
}

// Holds state relevant to move generation, to avoid redundant accesses/computations during move generation.
struct State {
    PinsInfo pins{};
    Bitboard occ{}, usOcc{}, themOcc{};
    Bitboard checkers{};
    Bitboard evasionMask{};
    Color us{}, them{};
    Square kingSq{};
    uint8_t numCheckers{};
};

State init_state(const Position& pos) noexcept {
    State state{};

    state.us = pos.sideToMove();
    state.them = ~state.us;
    state.kingSq = pos.kingSquare(state.us);
    state.occ = pos.occupancy();
    state.usOcc = pos.occupancy(state.us);
    state.themOcc = pos.occupancy(state.them);
    state.checkers = checkers(pos, state.us);
    state.numCheckers = bit_count(state.checkers);
    state.pins = compute_pins(pos, state.us);
    state.evasionMask = (state.numCheckers == 1) ? evasion_mask(pos, state.kingSq, state.checkers) : ~Bitboard{ 0 };

    return state;
}

void generate_king_moves(const Position& pos, const State& state, MoveList& moveList, GenMode mode) noexcept {
    Bitboard targets = attacks::king_attacks(state.kingSq) & ~state.usOcc;
    if (mode == GenMode::Tactical)
        targets &= state.themOcc;
    while (targets) {
        const auto to = static_cast<Square>(pop_lsb(targets));
        // Build occupancy after king moves (remove king, capture if enemy present, place king)
        Bitboard afterKingMoveOcc = state.occ;
        afterKingMoveOcc ^= bitboard(state.kingSq);
        if (state.themOcc & bitboard(to))
            afterKingMoveOcc ^= bitboard(to);
        afterKingMoveOcc |= bitboard(to);
        // Ensure king doesn't move into check
        if (!is_square_attacked(pos, to, state.them, afterKingMoveOcc)) {
            const MoveType moveType = (state.themOcc & bitboard(to)) ? MoveType::Capture : MoveType::Normal;
            moveList.push_back(Move(state.kingSq, to, moveType));
        }
    }
}

template <PieceType PT>
void generate_piece_moves(const Position& pos, const State& state, MoveList& moveList, GenMode mode) {
    Bitboard pieces = pos.get(state.us, PT);
    while (pieces) {
        const auto from = static_cast<Square>(pop_lsb(pieces));
        Bitboard targets = attacks::piece_attacks<PT>(from, state.occ) & ~state.usOcc;

        targets &= state.pins.pinRay[to_underlying(from)] & state.evasionMask;
        if (mode == GenMode::Tactical)
            targets &= state.themOcc;

        push_simple_moves(from, targets, state.themOcc, moveList);
    }
}

void push_pawn_attacks(
    Square from,
    Bitboard targets,
    Bitboard themOcc,
    Color us,
    MoveType baseMoveType,
    MoveList& moveList
) noexcept {
    const Bitboard promotionMask = us == Color::White ? bitboard(Rank::R8) : bitboard(Rank::R1);
    Bitboard promoTargets = targets & promotionMask;
    Bitboard nonPromoTargets = targets & ~promotionMask;

    while (promoTargets) {
        const auto to = static_cast<Square>(pop_lsb(promoTargets));
        const bool isCapture = (themOcc & bitboard(to)) != 0;
        push_promotions(from, to, isCapture, moveList);
    }
    while (nonPromoTargets) {
        const auto to = static_cast<Square>(pop_lsb(nonPromoTargets));
        const MoveType moveType = (themOcc & bitboard(to)) ? MoveType::Capture : baseMoveType;
        moveList.push_back(Move(from, to, moveType));
    }
}

void generate_pawn_moves(const Position& pos, const State& state, MoveList& moveList, GenMode mode) noexcept {
    const Bitboard pawns = pos.get(state.us, PieceType::Pawn);
    const Bitboard promotionMask = (state.us == Color::White) ? bitboard(Rank::R8) : bitboard(Rank::R1);
    // Captures
    {
        Bitboard p = pawns;
        while (p) {
            const auto from = static_cast<Square>(pop_lsb(p));
            Bitboard targets = attacks::pawn_attacks(state.us, from) & state.themOcc;
            targets &= state.pins.pinRay[to_underlying(from)] & state.evasionMask;
            push_pawn_attacks(from, targets, state.themOcc, state.us, MoveType::Normal, moveList);
        }
    }
    // Pushes
    if (includes_quiets(mode)) {
        Bitboard p = pawns;
        while (p) {
            // Single push
            const auto from = static_cast<Square>(pop_lsb(p));
            const Bitboard filter = state.pins.pinRay[to_underlying(from)] & state.evasionMask;
            const Direction dir = (state.us == Color::White) ? Direction::North : Direction::South;
            const Square singlePushTo = geom::step(from, dir);
            if (!is_valid(singlePushTo) || (state.occ & bitboard(singlePushTo)) != 0)
                continue;

            const Bitboard singleTargets = bitboard(singlePushTo) & filter;
            push_pawn_attacks(from, singleTargets, state.themOcc, state.us, MoveType::Normal, moveList);

            // Double push (if on starting rank and single push was also valid)
            const Rank startRank = (state.us == Color::White) ? Rank::R2 : Rank::R7;
            if (rank(from) != startRank)
                continue;

            const Square doublePushTo = geom::step(singlePushTo, dir);
            if (!is_valid(doublePushTo) || (state.occ & bitboard(doublePushTo)) != 0)
                continue;
            const Bitboard doubleTargets = bitboard(doublePushTo) & filter;
            push_pawn_attacks(from, doubleTargets, state.themOcc, state.us, MoveType::PawnDoubleStep, moveList);
        }
    }
    // Non-capturing promotions
    else {
        Bitboard p = pawns;
        while (p) {
            const auto from = static_cast<Square>(pop_lsb(p));
            const Bitboard filter = state.pins.pinRay[to_underlying(from)] & state.evasionMask;
            const Direction dir = (state.us == Color::White) ? Direction::North : Direction::South;
            const Square singlePushTo = geom::step(from, dir);
            if (!is_valid(singlePushTo) || (state.occ & bitboard(singlePushTo)) != 0)
                continue;

            const Bitboard promoTargets = bitboard(singlePushTo) & filter & promotionMask;
            push_pawn_attacks(from, promoTargets, state.themOcc, state.us, MoveType::Normal, moveList);
        }
    }
}

void generate_castling_moves(const Position& pos, const State& state, MoveList& moveList, GenMode mode) noexcept {
    if (mode != GenMode::Legal || state.numCheckers > 0)
        return;

    const CastlingRights mask = (state.us == Color::White)
                                    ? (CastlingRights::WhiteKingside | CastlingRights::WhiteQueenside)
                                    : (CastlingRights::BlackKingside | CastlingRights::BlackQueenside);
    const CastlingRights cr = pos.castlingRights() & mask;
    if (cr == CastlingRights::None)
        return;

    if (state.us == Color::White) {
        if (has_right(cr, CastlingRights::WhiteKingside)) {
            const Square from = Square::E1;
            const Square to = Square::G1;
            const Bitboard fullPath = geom::between_or_to(from, to);
            if ((state.occ & fullPath) == 0 && !is_any_square_attacked(pos, fullPath, state.them)) {
                moveList.push_back(Move(from, to, MoveType::CastleKing));
            }
        }
        if (has_right(cr, CastlingRights::WhiteQueenside)) {
            const Square from = Square::E1;
            const Square to = Square::C1;
            // Ensure the full path is clear, and that the king's path isn't attacked
            const Bitboard kingPath = geom::between_or_to(from, to);
            const Bitboard fullPath = geom::between_or_to(from, Square::B1);
            if ((state.occ & fullPath) == 0 && !is_any_square_attacked(pos, kingPath, state.them)) {
                moveList.push_back(Move(from, to, MoveType::CastleQueen));
            }
        }
    }
    else {
        if (has_right(cr, CastlingRights::BlackKingside)) {
            const Square from = Square::E8;
            const Square to = Square::G8;
            const Bitboard fullPath = geom::between_or_to(from, to);
            if ((state.occ & fullPath) == 0 && !is_any_square_attacked(pos, fullPath, state.them)) {
                moveList.push_back(Move(from, to, MoveType::CastleKing));
            }
        }
        if (has_right(cr, CastlingRights::BlackQueenside)) {
            const Square from = Square::E8;
            const Square to = Square::C8;
            // Ensure the full path is clear, and that the king's path isn't attacked
            const Bitboard kingPath = geom::between_or_to(from, to);
            const Bitboard fullPath = geom::between_or_to(from, Square::B8);
            if ((state.occ & fullPath) == 0 && !is_any_square_attacked(pos, kingPath, state.them)) {
                moveList.push_back(Move(from, to, MoveType::CastleQueen));
            }
        }
    }
}

void generate_en_passant_moves(const Position& pos, const State& state, MoveList& moveList, GenMode mode) noexcept {
    (void)mode;
    const Square epSq = pos.epSquare();
    if (!is_valid(epSq))
        return;

    const Square kingSq = state.kingSq;
    const Color them = state.them;
    Bitboard pawns = pos.get(state.us, PieceType::Pawn) & attacks::pawn_attacks(them, epSq);

    while (pawns) {
        const auto from = static_cast<Square>(pop_lsb(pawns));
        // Only push the move if the en passant capture wouldn't expose the king to check
        const Direction dir = (state.us == Color::White) ? Direction::South : Direction::North;
        const Square capturedPawnSq = geom::step(epSq, dir);
        const Bitboard afterEpOcc = (state.occ ^ bitboard(from) ^ bitboard(capturedPawnSq)) | bitboard(epSq);
        if (!(attacks::bishop_attacks(kingSq, afterEpOcc) & pos.get(them, PieceType::Bishop)) &&
            !(attacks::rook_attacks(kingSq, afterEpOcc) & pos.get(them, PieceType::Rook)) &&
            !(attacks::queen_attacks(kingSq, afterEpOcc) & pos.get(them, PieceType::Queen))) [[likely]] {
            moveList.push_back(Move(from, epSq, MoveType::EnPassant));
        }
    }
}

}  // namespace

Bitboard checkers(const Position& pos, Color us) noexcept {
    const Color them = ~us;
    const Square kingSq = pos.kingSquare(us);
    const Bitboard occupied = pos.occupancy();

    Bitboard checkers = 0;
    checkers |= attacks::pawn_attacks(us, kingSq) & pos.get(them, PieceType::Pawn);
    checkers |= attacks::knight_attacks(kingSq) & pos.get(them, PieceType::Knight);
    checkers |= attacks::bishop_attacks(kingSq, occupied) & pos.get(them, PieceType::Bishop);
    checkers |= attacks::rook_attacks(kingSq, occupied) & pos.get(them, PieceType::Rook);
    checkers |= attacks::queen_attacks(kingSq, occupied) & pos.get(them, PieceType::Queen);
    return checkers;
}

bool is_square_attacked(const Position& pos, Square sq, Color by, Bitboard occupied) noexcept {
    return (attacks::pawn_attacks(~by, sq) & pos.get(by, PieceType::Pawn)) ||
           (attacks::knight_attacks(sq) & pos.get(by, PieceType::Knight)) ||
           (attacks::bishop_attacks(sq, occupied) & pos.get(by, PieceType::Bishop)) ||
           (attacks::rook_attacks(sq, occupied) & pos.get(by, PieceType::Rook)) ||
           (attacks::queen_attacks(sq, occupied) & pos.get(by, PieceType::Queen)) ||
           (attacks::king_attacks(sq) & pos.get(by, PieceType::King));
}

bool is_square_attacked(const Position& pos, Square sq, Color by) noexcept {
    return is_square_attacked(pos, sq, by, pos.occupancy());
}

bool is_any_square_attacked(const Position& pos, Bitboard b, Color by) noexcept {
    while (b) {
        const auto sq = static_cast<Square>(pop_lsb(b));
        if (is_square_attacked(pos, sq, by))
            return true;
    }
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

void generate_moves(const Position& pos, MoveList& moveList, GenMode mode) noexcept {
    moveList.clear();
    const State state = init_state(pos);
    const GenMode effectiveMode = (state.numCheckers > 0 && mode == GenMode::Tactical) ? GenMode::Evasions : mode;

    generate_king_moves(pos, state, moveList, effectiveMode);
    // Only king moves possible in double check
    if (state.numCheckers >= 2)
        return;

    generate_piece_moves<PieceType::Knight>(pos, state, moveList, effectiveMode);
    generate_piece_moves<PieceType::Bishop>(pos, state, moveList, effectiveMode);
    generate_piece_moves<PieceType::Rook>(pos, state, moveList, effectiveMode);
    generate_piece_moves<PieceType::Queen>(pos, state, moveList, effectiveMode);

    generate_pawn_moves(pos, state, moveList, effectiveMode);

    generate_castling_moves(pos, state, moveList, effectiveMode);
    generate_en_passant_moves(pos, state, moveList, effectiveMode);
}