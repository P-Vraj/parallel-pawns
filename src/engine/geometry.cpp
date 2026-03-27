#include "geometry.h"

#include "move_gen/attacks.h"

namespace geom {

std::array<std::array<Bitboard, 64>, 64> line_table{};
std::array<std::array<Bitboard, 64>, 64> between_table{};
std::array<std::array<Bitboard, 64>, 64> ray_pass_table{};

void init_geometry_tables() noexcept {
    for (size_t i = 0; i < 64; ++i) {
        const auto from = static_cast<Square>(i);
        const Bitboard fromBB = bitboard(from);

        const Bitboard orthogonalFrom = attacks::rook_attacks(from, 0);
        const Bitboard diagonalFrom = attacks::bishop_attacks(from, 0);

        for (size_t j = 0; j < 64; ++j) {
            const auto to = static_cast<Square>(j);
            const Bitboard toBB = bitboard(to);

            Bitboard line = 0;
            Bitboard between = 0;
            Bitboard rayPass = 0;
            if (orthogonalFrom & toBB) {
                const Bitboard orthogonalTo = attacks::rook_attacks(to, 0);

                line = (orthogonalFrom & orthogonalTo) | fromBB | toBB;
                between = attacks::rook_attacks(from, toBB) & attacks::rook_attacks(to, fromBB);
                rayPass = orthogonalFrom & (attacks::rook_attacks(to, fromBB) | toBB);
            }
            else if (diagonalFrom & toBB) {
                const Bitboard diagonalTo = attacks::bishop_attacks(to, 0);

                line = (diagonalFrom & diagonalTo) | fromBB | toBB;
                between = attacks::bishop_attacks(from, toBB) & attacks::bishop_attacks(to, fromBB);
                rayPass = diagonalFrom & (attacks::bishop_attacks(to, fromBB) | toBB);
            }

            line_table[i][j] = line;
            between_table[i][j] = between;
            ray_pass_table[i][j] = rayPass;
        }
    }
}

};  // namespace geom
