#pragma once
#include <array>
#include <cstdint>
#include <utility>

using std::to_underlying;

using Bitboard = uint64_t;

enum class Square : uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    None = 64,

    Count = 64
};

enum class Direction : int8_t {
    North       = +8,
    South       = -8,
    East        = +1,
    West        = -1,
    NorthEast   = +9,
    NorthWest   = +7,
    SouthEast   = -7,
    SouthWest   = -9,

    Count = 8,
};

constexpr inline Square operator+(Square sq, Direction dir) noexcept {
    return static_cast<Square>(to_underlying(sq) + to_underlying(dir));
}

constexpr inline Square operator+(Direction dir, Square sq) noexcept { return sq + dir; }

constexpr inline Square& operator+=(Square& sq, Direction dir) noexcept { return sq = sq + dir; }

constexpr inline Square& operator++(Square& sq) noexcept {
    return sq = static_cast<Square>(to_underlying(sq) + 1);
}

constexpr inline Square operator-(Square sq, Direction dir) noexcept {
    return static_cast<Square>(to_underlying(sq) - to_underlying(dir));
}

constexpr inline Square& operator-=(Square& sq, Direction dir) noexcept { return sq = sq - dir; }

constexpr inline Square& operator--(Square& sq) noexcept {
    return sq = static_cast<Square>(to_underlying(sq) - 1);
}

enum class File : uint8_t {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,

    Count = 8,
};

constexpr File file(Square sq) noexcept { return File(to_underlying(sq) % 8); }

constexpr bool isValid(File f) noexcept {
    return to_underlying(f) < to_underlying(File::Count);
}

constexpr inline File& operator+=(File& f, int rhs) noexcept {
    f = static_cast<File>(static_cast<int>(to_underlying(f)) + rhs);
    return f;
}

constexpr inline File operator+(File lhs, int rhs) noexcept { lhs += rhs; return lhs; }

constexpr inline File operator+(int lhs, File rhs) noexcept { return rhs + lhs; }

constexpr inline File& operator++(File& f) noexcept { return f += 1; }

constexpr inline File& operator-=(File& f, int rhs) noexcept {
    f = static_cast<File>(static_cast<int>(to_underlying(f)) - rhs);
    return f;
}

constexpr inline File operator-(File lhs, int rhs) noexcept { lhs -= rhs; return lhs; }

constexpr inline File& operator--(File& f) noexcept { return f -= 1; }

enum class Rank : uint8_t {
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,

    Count = 8,
};

// Returns the 0-indexed rank of a square
constexpr Rank rank(Square sq) noexcept { return Rank(to_underlying(sq) / 8); }

constexpr bool isValid(Rank r) noexcept {
    return to_underlying(r) < to_underlying(Rank::Count);
}

constexpr inline Rank& operator+=(Rank& r, int rhs) noexcept {
    r = static_cast<Rank>(static_cast<int>(to_underlying(r)) + rhs);
    return r;
}

constexpr inline Rank operator+(Rank lhs, int rhs) noexcept { lhs += rhs; return lhs; }

constexpr inline Rank operator+(int lhs, Rank rhs) noexcept { return rhs + lhs; }

constexpr inline Rank& operator++(Rank& r) noexcept { return r += 1; }

constexpr inline Rank& operator-=(Rank& r, int rhs) noexcept {
    r = static_cast<Rank>(static_cast<int>(to_underlying(r)) - rhs);
    return r;
}

constexpr inline Rank operator-(Rank lhs, int rhs) noexcept { lhs -= rhs; return lhs; }

constexpr inline Rank& operator--(Rank& r) noexcept { return r -= 1; }

constexpr Square make_square(File f, Rank r) noexcept {
    return Square(to_underlying(f) | (to_underlying(r) << 3));
}

constexpr bool isValid(File f, Rank r) noexcept {
    return isValid(f) && isValid(r);
}

constexpr inline Bitboard bitboard(Square sq) noexcept {
    return 1ULL << to_underlying(sq);
}

enum class Color : uint8_t {
    White,
    Black,

    Count = 2
};

constexpr Color operator~(Color c) noexcept {
    return c == Color::White ? Color::Black : Color::White; 
}

constexpr size_t idx(Color c) noexcept {
    return static_cast<size_t>(c);
}

enum class PieceType : uint8_t {
    None,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,

    Count = 7
};

constexpr size_t idx(PieceType pt) noexcept {
    return static_cast<size_t>(pt);
}

enum class Piece : uint8_t {
    None,
    WPawn = to_underlying(PieceType::Pawn),     WKnight, WBishop, WRook, WQueen, WKing,
    BPawn = to_underlying(PieceType::Pawn) + 8, BKnight, BBishop, BRook, BQueen, BKing,
    
    Count = 16
};

constexpr Color color(Piece p) noexcept {
    return Color(to_underlying(p) / 8);
}

constexpr PieceType pieceType(Piece p) noexcept {
    return PieceType(to_underlying(p) % 8);
}

constexpr Piece make_piece(Color c, PieceType pt) noexcept {
    return Piece(to_underlying(pt) + (to_underlying(c) * 8));
}

enum class MoveType : uint8_t {
    Normal                      = 0b0000,
    PawnDoubleStep              = 0b0001,
    CastleKing                  = 0b0010,
    CastleQueen                 = 0b0011,
    PromotionKnight             = 0b0100,
    PromotionBishop             = 0b0101,
    PromotionRook               = 0b0110,
    PromotionQueen              = 0b0111,
    Capture                     = 0b1000,
    EnPassant                   = 0b1001,
    // Unused                   = 0b1010,
    // Unused                   = 0b1011,
    PromotionCaptureKnight      = 0b1100,
    PromotionCaptureBishop      = 0b1101,
    PromotionCaptureRook        = 0b1110,
    PromotionCaptureQueen       = 0b1111,

    Count = 14,
};

struct Move {
    // A move is packed into 16 bits as follows:
    // Bits 0-5:    From square
    // Bits 6-11:   To square
    // Bits 12-15:  Move type

    constexpr explicit Move(uint16_t data)
        : data_(data) {}
    constexpr Move(Square from, Square to)
        : data_(to_underlying(from) | (to_underlying(to) << 6)) {}
    constexpr Move(Square from, Square to, MoveType moveType)
        : data_(to_underlying(from) | (to_underlying(to) << 6) | (to_underlying(moveType) << 12)) {}
    
    constexpr uint16_t raw() const {
        return data_;
    }
    constexpr bool operator==(const Move& other) const {
        return data_ == other.data_;
    }
    constexpr bool operator!=(const Move& other) const {
        return data_ != other.data_;
    }
    constexpr explicit operator bool() const {
        return data_ != 0;
    }
    
    constexpr Square from() const {
        return static_cast<Square>(data_ & 63);
    }
    constexpr Square to() const {
        return static_cast<Square>((data_ >> 6) & 63);
    }
    constexpr MoveType moveType() const {
        return static_cast<MoveType>(data_ >> 12);
    }
    static constexpr Move none() {
        return Move(0);
    }
    constexpr bool isNormal() const {
        return (data_ >> 12) == to_underlying(MoveType::Normal); 
    }
    constexpr bool isCapture() const {
        return data_ & (0b1000 << 12);
    }
    constexpr bool isPromotion() const {
        return data_ & (0b0100 << 12);
    }
    // Only call this if `isPromotion()` == true
    constexpr PieceType promotionType() const {
        return static_cast<PieceType>((data_ >> 12 & 0b0011) + to_underlying(PieceType::Knight));
    }

private:
    uint16_t data_;
};


struct Board {
    template <Color C, PieceType PT>
    constexpr Bitboard get() const {
        static_assert(C < Color::Count);
        static_assert(PT >= PieceType::Pawn && PT <= PieceType::King);
        return pieces_[idx(C)][idx(PT) - 1];
    }

    constexpr Bitboard get(Color c, PieceType pt) const {
        return pieces_[idx(c)][idx(pt) - 1];
    }

    template<Color C>
    constexpr Bitboard occupancy() const {
        static_assert(C < Color::Count);
        return colorOccupied_[idx(C)];
    }

    constexpr Bitboard occupancy() const {
        return occupied_;
    }

    // Recomputes occupancy bitboards (used during changes not reachable through move/unmove)
    void recomputeOccupancy() {
        for (size_t c = 0; c < idx(Color::Count); ++c) {
            Bitboard occ = 0;
            for (size_t pt = idx(PieceType::Pawn); pt < idx(PieceType::Count); ++pt) {
                occ |= get(Color(c), PieceType(pt));
            }
            colorOccupied_[c] = occ;
        }
        occupied_ = occupancy<Color::White>() | occupancy<Color::Black>();
    }

private:
    std::array<std::array<Bitboard, idx(PieceType::Count) - 1>, idx(Color::Count)> pieces_;
    std::array<Bitboard, idx(Color::Count)> colorOccupied_;
    Bitboard occupied_;
};