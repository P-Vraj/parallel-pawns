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

static constexpr std::array<uint8_t, 64> kCastleMask = make_castle_mask();
}  // namespace

Position Position::fromFEN(std::string_view fen) {
    Position pos{};
    std::vector<std::string_view> fields;
    for (auto field : fen | std::views::split(' ')) {
        fields.emplace_back(std::string_view(&*field.begin(), std::ranges::distance(field)));
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

    for (char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += (c - '0');
            continue;
        }
        Piece piece = to_piece(c);
        Square sq = make_square(static_cast<File>(file), static_cast<Rank>(rank));

        pieceMap_[to_underlying(sq)] = piece;
        file++;
    }
}

void Position::parseCastlingRights_(std::string_view castling) noexcept {
    castlingRights_ = CastlingRights::None;
    for (char c : castling) {
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
        Piece piece = pieceMap_[sq];
        if (piece == Piece::None)
            continue;

        Color c = color(piece);
        PieceType pt = piece_type(piece);
        set_bit(pieces_[to_underlying(c)][to_underlying(pt) - 1], static_cast<Square>(sq));
        set_bit(colorOccupied_[to_underlying(c)], static_cast<Square>(sq));
        set_bit(occupied_, static_cast<Square>(sq));
    }
}