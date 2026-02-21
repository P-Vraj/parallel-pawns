#include "position.h"

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
}