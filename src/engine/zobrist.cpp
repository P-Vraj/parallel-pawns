#include "zobrist.h"

namespace zobrist {

std::array<std::array<std::array<Key, 64>, to_underlying(PieceType::Count) - 1>, 2> piece;
std::array<Key, to_underlying(CastlingRights::Count)> castling;
std::array<Key, 8> enPassantFile;
Key side;

namespace {
// Using the SplitMix64 algorithm as a PRNG for generating the random numbers for the Zobrist keys
// Learn more here: https://prng.di.unimi.it/splitmix64.c
inline Key prng(uint64_t& state) noexcept {
    uint64_t k = (state += 0x9E3779B97F4A7C15ULL);
    k = (k ^ (k >> 30)) * 0xBF58476D1CE4E5B9ULL;
    k = (k ^ (k >> 27)) * 0x94D049BB133111EBULL;
    return static_cast<Key>(k ^ (k >> 31));
}
}  // namespace

void init_zobrist(uint64_t seed) noexcept {
    for (size_t c = 0; c < 2; ++c) {
        for (size_t pt = to_underlying(PieceType::Pawn); pt < to_underlying(PieceType::Count); ++pt) {
            for (size_t sq = 0; sq < 64; ++sq) {
                piece[c][pt - 1][sq] = prng(seed);
            }
        }
    }

    for (size_t i = 0; i < to_underlying(CastlingRights::Count); ++i) {
        castling[i] = prng(seed);
    }

    for (size_t f = 0; f < 8; ++f) {
        enPassantFile[f] = prng(seed);
    }

    side = prng(seed);
}

}  // namespace zobrist