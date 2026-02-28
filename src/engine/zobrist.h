#pragma once
#include "types.h"

namespace zobrist {

extern std::array<std::array<std::array<Key, 64>, to_underlying(PieceType::Count) - 1>, 2> piece;
extern std::array<Key, to_underlying(CastlingRights::Count)> castling;
extern std::array<Key, 8> enPassantFile;
extern Key side;

void init_zobrist(uint64_t seed = 0x9E3779B97F4A7C15ULL) noexcept;

}  // namespace zobrist