#include <vector>
#include <ranges>
#include "position.h"
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
    pos.fillPieceBitboards_();
    pos.recomputeOccupancy_();

    pos.kingSquare_[to_underlying(Color::Black)] = Square(get_lsb(pos.get(Color::Black, PieceType::King)));
    pos.kingSquare_[to_underlying(Color::White)] = Square(get_lsb(pos.get(Color::White, PieceType::King)));

    pos.sideToMove_ = (fields[1] == "w") ? Color::White : Color::Black;

    pos.parseCastlingRights_(fields[2]);

    pos.enPassantSquare_ = (fields[3] == "-") ? Square::None : to_square(fields[3]);

    pos.halfmoveClock_ = std::stoi(std::string(fields[4]));
    pos.fullmoveNumber_ = std::stoi(std::string(fields[5]));

    return pos;
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

void Position::fillPieceBitboards_() noexcept {
    for (size_t sq = 0; sq < 64; ++sq) {
        Piece piece = pieceMap_[sq];
        if (piece == Piece::None)
            continue;

        Color c = color(piece);
        PieceType pt = piece_type(piece);
        set_bit(pieces_[to_underlying(c)][to_underlying(pt) - 1], static_cast<Square>(sq));
    }
}

void Position::recomputeOccupancy_() noexcept {
    for (size_t c = 0; c < to_underlying(Color::Count); ++c) {
        Bitboard occ = 0;
        for (size_t pt = to_underlying(PieceType::Pawn); pt < to_underlying(PieceType::Count); ++pt) {
            occ |= get(Color(c), PieceType(pt));
        }
        colorOccupied_[c] = occ;
    }
    occupied_ = occupancy<Color::White>() | occupancy<Color::Black>();
}