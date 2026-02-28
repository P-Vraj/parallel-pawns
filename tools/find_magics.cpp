// This script finds magic numbers for rook and bishop move generation
// Build and run with:
// `clang++ -std=c++23 -O3 -Wall -I. tools/find_magics.cpp -o build/find_magics`
// `./build/find_magics > src/engine/move_gen/magics_generated.h`

#include <format>
#include <random>
#include <span>

#include "../src/engine/move_gen/attacks_sliders.h"
#include "../src/engine/types.h"
#include "../src/engine/util.h"

static inline uint64_t sparse_rand64(std::mt19937_64& rng) noexcept {
    // Sparse randoms (few bits set) are more likely to produce good magic numbers
    return rng() & rng() & rng();
}

static Magic find_magic_for_square(
    Bitboard mask,
    int relevantBits,
    std::span<Bitboard> occs,
    std::span<Bitboard> atts,
    std::mt19937_64& rng
) {
    const size_t tableSize = 1u << relevantBits;
    std::vector<Bitboard> used(tableSize);

    while (true) {
        Magic magic{ mask, sparse_rand64(rng), static_cast<uint8_t>(64 - relevantBits) };

        std::fill(used.begin(), used.end(), 0ULL);

        bool collision = false;
        for (size_t i = 0; i < occs.size(); ++i) {
            const size_t idx = magic_index(magic, occs[i]);
            const Bitboard a = atts[i];

            if (used[idx] == 0ULL) {
                used[idx] = a;
            }
            else if (used[idx] != a) {
                collision = true;
                break;
            }
        }

        if (!collision)
            return magic;
    }
}

static std::array<Magic, 64> find_magics(PieceType pieceType) {
    std::array<Magic, 64> out{};

    std::random_device rd;
    std::mt19937_64 rng(rd());

    const bool isRook = pieceType == PieceType::Rook;

    for (size_t s = 0; s < 64; ++s) {
        const Square sq = static_cast<Square>(s);
        const Bitboard mask = isRook ? rook_mask(sq) : bishop_mask(sq);
        const int relevantBits = bit_count(mask);
        const size_t count = 1u << relevantBits;

        std::vector<Bitboard> occs;
        std::vector<Bitboard> atts;
        occs.reserve(count);
        atts.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const Bitboard occ = index_to_occupancy(mask, i);
            const Bitboard att = isRook ? rook_attacks_ray(sq, occ) : bishop_attacks_ray(sq, occ);
            occs.push_back(occ);
            atts.push_back(att);
        }

        const Magic magic = find_magic_for_square(mask, relevantBits, occs, atts, rng);
        out[s] = magic;
    }

    return out;
}

static std::string magics_to_string(std::string_view name, const std::array<Magic, 64>& arr) {
    std::string out{};

    out += std::format("constexpr inline std::array<Magic, 64> {} = {{{{\n", name);

    for (size_t i = 0; i < 64; ++i) {
        const Magic& m = arr[i];

        out += std::format(
            "    {{ 0x{:016x}, 0x{:016x}, {} }}{}{}\n", static_cast<uint64_t>(m.mask), m.magic,
            static_cast<unsigned>(m.shift), (i == 63 ? "" : ","), ""
        );
    }

    out += "}};\n";
    return out;
}

static std::string header_to_string() {
    std::string header{};
    header += "#pragma once\n";
    header += "#include <array>\n";
    header += "#include \"magic_types.h\"\n";
    return header;
}

int main() {
    auto rookMagics = find_magics(PieceType::Rook);
    auto bishopMagics = find_magics(PieceType::Bishop);

    std::string out{};
    out += header_to_string() + "\n";
    out += magics_to_string("kRookMagics", rookMagics) + "\n";
    out += magics_to_string("kBishopMagics", bishopMagics);
    std::cout << out;

    return 0;
}