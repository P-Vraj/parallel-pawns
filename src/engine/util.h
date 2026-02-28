#pragma once
#include <format>
#include <iostream>
#include <string>
#include <utility>

#include "types.h"

inline std::string to_string(Color c) {
    return c == Color::White ? "w" : "b";
}

inline char to_char(PieceType pt) {
    switch (pt) {
        case PieceType::Pawn:
            return 'p';
        case PieceType::Knight:
            return 'n';
        case PieceType::Bishop:
            return 'b';
        case PieceType::Rook:
            return 'r';
        case PieceType::Queen:
            return 'q';
        case PieceType::King:
            return 'k';
        default:
            return '.';
    }
    return '?';
}

inline std::string to_string(Piece p) {
    char pt = to_char(piece_type(p));
    if (color(p) == Color::White) {
        return std::string{ static_cast<char>(toupper(pt)) };
    }
    return std::string{ pt };
}

inline std::string to_string(Square sq) {
    if (sq == Square::None) {
        return "0000";  // UCI nullmove
    }
    char f = 'a' + to_underlying(file(sq));
    char r = '1' + to_underlying(rank(sq));
    return std::string{ f, r };
}

// Long algebraic notation format
inline std::string to_string(Move m) {
    std::string promo = m.isPromotion() ? std::string{ to_char(m.promotionType()) } : "";
    return to_string(m.from()) + to_string(m.to()) + promo;
}

inline std::string to_string(Bitboard b) {
    std::string divider = "+---+---+---+---+---+---+---+---+";
    std::string out = divider + "\n";

    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            out += (b & bitboard(sq)) ? "| * " : "|   ";
        }
        out += "| " + std::string{ static_cast<char>('1' + r) };
        out += "\n" + divider + "\n";
    }

    out += "  a   b   c   d   e   f   g   h\n";
    return out;
}

inline std::string to_string(const Position& pos) {
    std::string divider = "+---+---+---+---+---+---+---+---+";
    std::string out = divider + "\n";

    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            Piece p = pos.pieceOn(sq);
            out += "| " + to_string(p) + " ";
        }
        out += "| " + std::string{ static_cast<char>('1' + r) };
        out += "\n" + divider + "\n";
    }

    out += "  a   b   c   d   e   f   g   h\n";
    return out;
}

constexpr PieceType to_piece_type(char c) {
    switch (tolower(c)) {
        case 'p':
            return PieceType::Pawn;
        case 'n':
            return PieceType::Knight;
        case 'b':
            return PieceType::Bishop;
        case 'r':
            return PieceType::Rook;
        case 'q':
            return PieceType::Queen;
        case 'k':
            return PieceType::King;
        default:
            return PieceType::None;
    }
}

constexpr Piece to_piece(char c) {
    if (c == '.' || c == '?') {
        return Piece::None;
    }
    Color color = isupper(c) ? Color::White : Color::Black;
    PieceType pt = to_piece_type(c);
    return make_piece(color, pt);
}

constexpr Square to_square(std::string_view str) {
    if (str == "0000") {
        return Square::None;  // UCI nullmove
    }
    File f = static_cast<File>(tolower(str[0]) - 'a');
    Rank r = static_cast<Rank>(str[1] - '1');
    return make_square(f, r);
}

constexpr inline void set_bit(Bitboard& b, Square sq) noexcept {
    b |= bitboard(sq);
}

constexpr inline void clear_bit(Bitboard& b, Square sq) noexcept {
    b &= ~bitboard(sq);
}

constexpr inline bool get_bit(Bitboard b, Square sq) noexcept {
    return b & bitboard(sq);
}

constexpr inline int get_lsb(Bitboard b) noexcept {
    return __builtin_ctzll(b);
}

constexpr inline int pop_lsb(Bitboard& b) noexcept {
    int lsbIndex = get_lsb(b);
    b &= b - 1;
    return lsbIndex;
}

constexpr inline int bit_count(Bitboard b) noexcept {
    return __builtin_popcountll(b);
}
