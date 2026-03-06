#include "position.h"

#include <ranges>
#include <vector>

#include "util.h"

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
    for (auto field : fen | std::views::split(' ')) {
        fields.emplace_back(&*field.begin(), std::ranges::distance(field));
    }

    pos.parsePieceMap_(fields[0]);
    pos.fillBitboards_();

    pos.kingSquare_[to_underlying(Color::Black)] = Square(get_lsb(pos.get(Color::Black, PieceType::King)));
    pos.kingSquare_[to_underlying(Color::White)] = Square(get_lsb(pos.get(Color::White, PieceType::King)));

    pos.sideToMove_ = (fields[1] == "w") ? Color::White : Color::Black;

    pos.parseCastlingRights_(fields[2]);

    pos.enPassantSquare_ = (fields[3] == "-") ? Square::None : to_square(fields[3]);

    pos.halfmoveClock_ = std::stoi(std::string(fields[4]));
    pos.fullmoveNumber_ = std::stoi(std::string(fields[5]));

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
    fen += std::format("{} ", (epSquare() == Square::None) ? "-" : to_string(epSquare()));
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

void Position::makeMove(Move m, UndoInfo& undo) noexcept {
    undo = UndoInfo{
        .captured = Piece::None,
        .castlingRights = castlingRights_,
        .epSquare = enPassantSquare_,
        .hash = hash_
    };
    
    const Square from = m.from();
    const Square to = m.to();
    const Piece movingPiece = pieceOn(from);

    enPassantSquare_ = Square::None;

    switch (m.moveType()) {
        case MoveType::Normal: {
            movePiece(from, to);
            break;
        }
        case MoveType::Capture: {
            undo.captured = pieceOn(to);
            removePiece(to);
            movePiece(from, to);
            break;
        }
        case MoveType::PawnDoubleStep: {
            movePiece(from, to);
            const Direction dir = (sideToMove_ == Color::White) ? Direction::North : Direction::South;
            enPassantSquare_ = to + dir;
            break;
        }
        case MoveType::EnPassant: {
            const Direction dir = (sideToMove_ == Color::White) ? Direction::South : Direction::North;
            const Square capturedPawnSquare = to + dir;
            undo.captured = pieceOn(capturedPawnSquare);
            removePiece(capturedPawnSquare);
            movePiece(from, to);
            break;
        }
        case MoveType::CastleKing: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::G1 : Square::G8;
            const Square rookFrom = white ? Square::H1 : Square::H8;
            const Square rookTo = white ? Square::F1 : Square::F8;

            movePiece(kingFrom, kingTo);
            movePiece(rookFrom, rookTo);
            break;
        }
        case MoveType::CastleQueen: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::C1 : Square::C8;
            const Square rookFrom = white ? Square::A1 : Square::A8;
            const Square rookTo = white ? Square::D1 : Square::D8;

            movePiece(kingFrom, kingTo);
            movePiece(rookFrom, rookTo);
            break;
        }
        case MoveType::PromotionKnight:
        case MoveType::PromotionBishop:
        case MoveType::PromotionRook:
        case MoveType::PromotionQueen: {
            removePiece(from);
            putPiece(to, make_piece(sideToMove_, m.promotionType()));
            break;
        }
        case MoveType::PromotionCaptureKnight:
        case MoveType::PromotionCaptureBishop:
        case MoveType::PromotionCaptureRook:
        case MoveType::PromotionCaptureQueen: {
            undo.captured = pieceOn(to);
            removePiece(to);
            removePiece(from);
            putPiece(to, make_piece(sideToMove_, m.promotionType()));
            break;
        }
        default:
            break;
    }

    updateCastlingRights(from, to, movingPiece);
    sideToMove_ = ~sideToMove_;
}

void Position::undoMove(Move m, const UndoInfo& undo) noexcept {
    castlingRights_ = undo.castlingRights;
    enPassantSquare_ = undo.epSquare;
    hash_ = undo.hash;
    
    sideToMove_ = ~sideToMove_;

    const Square from = m.from();
    const Square to = m.to();
    const Piece movingPiece = pieceOn(to);

    switch (m.moveType()) {
        case MoveType::Normal: {
            movePiece(to, from);
            break;
        }
        case MoveType::Capture: {
            movePiece(to, from);
            putPiece(to, undo.captured);
            break;
        }
        case MoveType::PawnDoubleStep: {
            movePiece(to, from);
            break;
        }
        case MoveType::EnPassant: {
            const Direction dir = (sideToMove_ == Color::White) ? Direction::South : Direction::North;
            const Square capturedPawnSquare = to + dir;
            movePiece(to, from);
            putPiece(capturedPawnSquare, undo.captured);
            break;
        }
        case MoveType::CastleKing: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::G1 : Square::G8;
            const Square rookFrom = white ? Square::H1 : Square::H8;
            const Square rookTo = white ? Square::F1 : Square::F8;

            movePiece(kingTo, kingFrom);
            movePiece(rookTo, rookFrom);
            break;
        }
        case MoveType::CastleQueen: {
            const bool white = (sideToMove_ == Color::White);
            const Square kingFrom = white ? Square::E1 : Square::E8;
            const Square kingTo = white ? Square::C1 : Square::C8;
            const Square rookFrom = white ? Square::A1 : Square::A8;
            const Square rookTo = white ? Square::D1 : Square::D8;

            movePiece(kingTo, kingFrom);
            movePiece(rookTo, rookFrom);
            break;
        }
        case MoveType::PromotionKnight:
        case MoveType::PromotionBishop:
        case MoveType::PromotionRook:
        case MoveType::PromotionQueen: {
            removePiece(to);
            putPiece(from, make_piece(sideToMove_, PieceType::Pawn));
            break;
        }
        case MoveType::PromotionCaptureKnight:
        case MoveType::PromotionCaptureBishop:
        case MoveType::PromotionCaptureRook:
        case MoveType::PromotionCaptureQueen: {
            removePiece(to);
            putPiece(from, make_piece(sideToMove_, PieceType::Pawn));
            putPiece(to, undo.captured);
            break;
        default:
            break;
        }
    }
}

void Position::removePiece(Square sq) noexcept {
    const Piece piece = pieceOn(sq);
    pieceMap_[to_underlying(sq)] = Piece::None;
    clear_bit(pieces_[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1], sq);
    clear_bit(colorOccupied_[to_underlying(color(piece))], sq);
    clear_bit(occupied_, sq);
}

void Position::putPiece(Square sq, Piece piece) noexcept {
    pieceMap_[to_underlying(sq)] = piece;
    set_bit(pieces_[to_underlying(color(piece))][to_underlying(piece_type(piece)) - 1], sq);
    set_bit(colorOccupied_[to_underlying(color(piece))], sq);
    set_bit(occupied_, sq);
}

void Position::movePiece(Square from, Square to) noexcept {
    const Piece piece = pieceOn(from);
    removePiece(from);
    putPiece(to, piece);
}

void Position::updateCastlingRights(Square from, Square to, Piece movingPiece) noexcept {
    const uint8_t mask = kCastleMask[to_underlying(from)] & kCastleMask[to_underlying(to)];
    castlingRights_ = castlingRights_ & static_cast<CastlingRights>(mask);
}