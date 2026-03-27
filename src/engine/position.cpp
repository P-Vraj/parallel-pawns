#include "position.h"

#include <algorithm>
#include <ranges>
#include <vector>

#include "move_gen/generator.h"
#include "util.h"
#include "zobrist.h"

namespace {
constexpr std::array<uint8_t, 64> make_castle_mask() {
    std::array<uint8_t, 64> mask{};
    mask.fill(to_underlying(CastlingRights::All));

    mask[to_underlying(Square::E1)] &= 0b1100;
    mask[to_underlying(Square::H1)] &= 0b1110;
    mask[to_underlying(Square::A1)] &= 0b1101;

    mask[to_underlying(Square::E8)] &= 0b0011;
    mask[to_underlying(Square::H8)] &= 0b1011;
    mask[to_underlying(Square::A8)] &= 0b0111;

    return mask;
}

constexpr std::array<uint8_t, 64> kCastleMask = make_castle_mask();
}  // namespace

Position Position::fromFEN(std::string_view fen) {
    Position pos{};
    std::vector<std::string_view> fields;
    for (const auto field : fen | std::views::split(' ')) {
        fields.emplace_back(&*field.begin(), std::ranges::distance(field));
    }

    pos.parsePieceMap_(fields[0]);
    pos.fillBitboards_();

    for (const Color c : { Color::White, Color::Black }) {
        pos.kingSquare_[to_underlying(c)] = static_cast<Square>(get_lsb(pos.get(c, PieceType::King)));
    }

    pos.sideToMove_ = (fields[1] == "w") ? Color::White : Color::Black;

    pos.parseCastlingRights_(fields[2]);

    pos.enPassantSquare_ = (fields[3] == "-") ? Square::None : to_square(fields[3]);

    if (fields.size() > 4)
        pos.halfmoveClock_ = std::stoi(std::string(fields[4]));
    if (fields.size() > 5)
        pos.fullmoveNumber_ = std::stoi(std::string(fields[5]));

    pos.hash_ = pos.computeHash();

    return pos;
}

std::string Position::toFEN() const {
    std::string fen{};

    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            const Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            const Piece p = pieceOn(sq);
            if (is_empty(p)) {
                empty++;
                continue;
            }
            if (empty > 0) {
                fen += std::to_string(empty);
                empty = 0;
            }
            fen += to_string(p);
        }
        if (empty > 0)
            fen += std::to_string(empty);
        if (r > 0)
            fen += '/';
    }
    fen += ' ';

    fen += std::format("{} ", (sideToMove() == Color::White) ? 'w' : 'b');
    fen += std::format("{} ", to_string(castlingRights()));
    fen += std::format("{} ", (hasLegalEnPassant_()) ? to_string(epSquare()) : "-");
    fen += std::format("{} {}", halfmoveClock(), fullmoveNumber());

    return fen;
}

void Position::parsePieceMap_(std::string_view placement) noexcept {
    int rank = 7;
    int file = 0;

    for (const char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += (c - '0');
            continue;
        }
        const Piece piece = to_piece(c);
        const Square sq = make_square(static_cast<File>(file), static_cast<Rank>(rank));

        pieceMap_[to_underlying(sq)] = piece;
        file++;
    }
}

void Position::parseCastlingRights_(std::string_view castling) noexcept {
    castlingRights_ = CastlingRights::None;
    for (const char c : castling) {
        switch (c) {
            case 'K':
                castlingRights_ |= CastlingRights::WhiteKingside;
                break;
            case 'Q':
                castlingRights_ |= CastlingRights::WhiteQueenside;
                break;
            case 'k':
                castlingRights_ |= CastlingRights::BlackKingside;
                break;
            case 'q':
                castlingRights_ |= CastlingRights::BlackQueenside;
                break;
            case '-':
            default:
                break;
        }
    }
}

void Position::fillBitboards_() noexcept {
    for (size_t sq = 0; sq < 64; ++sq) {
        const Piece piece = pieceMap_[sq];
        if (piece == Piece::None)
            continue;

        const Color c = color(piece);
        const PieceType pt = piece_type(piece);
        set_bit(pieces_[to_underlying(c)][to_underlying(pt) - 1], static_cast<Square>(sq));
        set_bit(colorOccupied_[to_underlying(c)], static_cast<Square>(sq));
        set_bit(occupied_, static_cast<Square>(sq));
    }
}

bool Position::hasLegalEnPassant_() const noexcept {
    if (!is_valid(enPassantSquare_))
        return false;
    Position opponent = *this;
    opponent.sideToMove_ = ~opponent.sideToMove_;
    MoveList moves(opponent);
    return std::ranges::any_of(moves, [](const Move m) { return m.moveType() == MoveType::EnPassant; });
}

void Position::makeMove(Move m, UndoInfo& undo) noexcept {
    undo = UndoInfo{
        .captured = Piece::None,
        .castlingRights = castlingRights_,
        .enPassantSquare = enPassantSquare_,
        .halfmoveClock = halfmoveClock_,
    };

    const Square from = m.from();
    const Square to = m.to();

    if (is_valid(enPassantSquare_))
        hash_ ^= zobrist::enPassantFile[to_underlying(file(enPassantSquare_))];
    enPassantSquare_ = Square::None;

    switch (m.moveType()) {
        case MoveType::Normal: {
            movePiece_(from, to);
            break;
        }
        case MoveType::Capture: {
            undo.captured = pieceOn(to);
            removePiece_(to);
            movePiece_(from, to);
            break;
        }
        case MoveType::PawnDoubleStep: {
            const Direction dir = (sideToMove_ == Color::White) ? Direction::South : Direction::North;
            enPassantSquare_ = to + dir;
            movePiece_(from, to);
            hash_ ^= zobrist::enPassantFile[to_underlying(file(enPassantSquare_))];
            break;
        }
        case MoveType::EnPassant: {
            const Direction dir = (sideToMove_ == Color::White) ? Direction::South : Direction::North;
            const Square capturedPawnSquare = to + dir;
            undo.captured = pieceOn(capturedPawnSquare);
            removePiece_(capturedPawnSquare);
            movePiece_(from, to);
            break;
        }
        case MoveType::CastleKing: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::G1 : Square::G8;
            const Square rookFrom = white ? Square::H1 : Square::H8;
            const Square rookTo = white ? Square::F1 : Square::F8;

            movePiece_(kingFrom, kingTo);
            movePiece_(rookFrom, rookTo);
            break;
        }
        case MoveType::CastleQueen: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::C1 : Square::C8;
            const Square rookFrom = white ? Square::A1 : Square::A8;
            const Square rookTo = white ? Square::D1 : Square::D8;

            movePiece_(kingFrom, kingTo);
            movePiece_(rookFrom, rookTo);
            break;
        }
        case MoveType::PromotionKnight:
        case MoveType::PromotionBishop:
        case MoveType::PromotionRook:
        case MoveType::PromotionQueen: {
            removePiece_(from);
            putPiece_(to, make_piece(sideToMove_, m.promotionType()));
            break;
        }
        case MoveType::PromotionCaptureKnight:
        case MoveType::PromotionCaptureBishop:
        case MoveType::PromotionCaptureRook:
        case MoveType::PromotionCaptureQueen: {
            undo.captured = pieceOn(to);
            removePiece_(to);
            removePiece_(from);
            putPiece_(to, make_piece(sideToMove_, m.promotionType()));
            break;
        }
        default:
            break;
    }

    // Update castling rights
    hash_ ^= zobrist::castling[to_underlying(castlingRights_)];
    const uint8_t mask = kCastleMask[to_underlying(from)] & kCastleMask[to_underlying(to)];
    castlingRights_ &= static_cast<CastlingRights>(mask);
    hash_ ^= zobrist::castling[to_underlying(castlingRights_)];

    // Update king square, move counters, and side to move
    kingSquare_[to_underlying(sideToMove_)] = static_cast<Square>(get_lsb(get(sideToMove_, PieceType::King)));

    fullmoveNumber_ += (sideToMove_ == Color::Black) ? 1 : 0;
    halfmoveClock_ = (m.isCapture() || piece_type(pieceOn(to)) == PieceType::Pawn) ? 0 : halfmoveClock_ + 1;

    sideToMove_ = ~sideToMove_;
    hash_ ^= zobrist::side;
}

void Position::undoMove(Move m, const UndoInfo& undo) noexcept {
    hash_ ^= zobrist::castling[to_underlying(castlingRights_)];
    castlingRights_ = undo.castlingRights;
    hash_ ^= zobrist::castling[to_underlying(castlingRights_)];

    if (is_valid(enPassantSquare_))
        hash_ ^= zobrist::enPassantFile[to_underlying(file(enPassantSquare_))];
    enPassantSquare_ = undo.enPassantSquare;
    if (is_valid(enPassantSquare_))
        hash_ ^= zobrist::enPassantFile[to_underlying(file(enPassantSquare_))];

    hash_ ^= zobrist::side;
    sideToMove_ = ~sideToMove_;

    halfmoveClock_ = undo.halfmoveClock;
    fullmoveNumber_ -= (sideToMove_ == Color::Black) ? 1 : 0;

    const Square from = m.from();
    const Square to = m.to();

    switch (m.moveType()) {
        case MoveType::Normal: {
            movePiece_(to, from);
            break;
        }
        case MoveType::Capture: {
            movePiece_(to, from);
            putPiece_(to, undo.captured);
            break;
        }
        case MoveType::PawnDoubleStep: {
            movePiece_(to, from);
            break;
        }
        case MoveType::EnPassant: {
            const Direction dir = (sideToMove_ == Color::White) ? Direction::South : Direction::North;
            const Square capturedPawnSquare = to + dir;
            movePiece_(to, from);
            putPiece_(capturedPawnSquare, undo.captured);
            break;
        }
        case MoveType::CastleKing: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::G1 : Square::G8;
            const Square rookFrom = white ? Square::H1 : Square::H8;
            const Square rookTo = white ? Square::F1 : Square::F8;

            movePiece_(kingTo, kingFrom);
            movePiece_(rookTo, rookFrom);
            break;
        }
        case MoveType::CastleQueen: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::C1 : Square::C8;
            const Square rookFrom = white ? Square::A1 : Square::A8;
            const Square rookTo = white ? Square::D1 : Square::D8;

            movePiece_(kingTo, kingFrom);
            movePiece_(rookTo, rookFrom);
            break;
        }
        case MoveType::PromotionKnight:
        case MoveType::PromotionBishop:
        case MoveType::PromotionRook:
        case MoveType::PromotionQueen: {
            removePiece_(to);
            putPiece_(from, make_piece(sideToMove_, PieceType::Pawn));
            break;
        }
        case MoveType::PromotionCaptureKnight:
        case MoveType::PromotionCaptureBishop:
        case MoveType::PromotionCaptureRook:
        case MoveType::PromotionCaptureQueen: {
            removePiece_(to);
            putPiece_(from, make_piece(sideToMove_, PieceType::Pawn));
            putPiece_(to, undo.captured);
            break;
        }
        default:
            break;
    }
    kingSquare_[to_underlying(sideToMove_)] = static_cast<Square>(get_lsb(get(sideToMove_, PieceType::King)));
}

void Position::removePiece_(Square sq) noexcept {
    const Piece piece = pieceOn(sq);
    pieceMap_[to_underlying(sq)] = Piece::None;
    clear_bit(pieces_[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1], sq);
    clear_bit(colorOccupied_[to_underlying(color(piece))], sq);
    clear_bit(occupied_, sq);
    hash_ ^= zobrist::piece[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1][to_underlying(sq)];
}

void Position::putPiece_(Square sq, Piece piece) noexcept {
    pieceMap_[to_underlying(sq)] = piece;
    set_bit(pieces_[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1], sq);
    set_bit(colorOccupied_[to_underlying(color(piece))], sq);
    set_bit(occupied_, sq);
    hash_ ^= zobrist::piece[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1][to_underlying(sq)];
}

void Position::movePiece_(Square from, Square to) noexcept {
    const Piece piece = pieceOn(from);
    removePiece_(from);
    putPiece_(to, piece);
}

bool Position::inCheck() const noexcept {
    return is_square_attacked(*this, kingSquare(sideToMove_), ~sideToMove_);
}

Key Position::computeHash() const noexcept {
    Key h = 0;
    for (size_t s = 0; s < 64; ++s) {
        const auto sq = static_cast<Square>(s);
        const Piece piece = pieceOn(sq);
        if (!is_empty(piece))
            h ^= zobrist::piece[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1][to_underlying(sq)];
    }
    h ^= zobrist::castling[to_underlying(castlingRights())];
    if (is_valid(enPassantSquare_))
        h ^= zobrist::enPassantFile[to_underlying(file(enPassantSquare_))];
    if (sideToMove_ == Color::Black)
        h ^= zobrist::side;
    return h;
}
