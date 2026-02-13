#include <iostream>
#include "types.h"
#include "util.h"
#include "move_gen/attacks.h"

int main() {
    init_attack_tables();

    Bitboard blockers = bitboard(Square::B2) | bitboard(Square::F1);
    std::cout << "Blockers:\n" << to_string(blockers) << '\n';
    std::cout << "King attacks:\n" << to_string(king_attacks(Square::D4)) << '\n';
    std::cout << "White pawn attacks:\n" << to_string(pawn_attacks(Color::White, Square::D4)) << '\n';
    std::cout << "Black pawn attacks:\n" << to_string(pawn_attacks(Color::Black, Square::D4)) << '\n';
    std::cout << "Knight attacks:\n" << to_string(knight_attacks(Square::D4)) << '\n';
    std::cout << "Bishop attacks:\n" << to_string(bishop_attacks(Square::C1, blockers)) << '\n';
    std::cout << "Rook attacks:\n" << to_string(rook_attacks(Square::A1, blockers)) << '\n';
    std::cout << "Queen attacks:\n" << to_string(queen_attacks(Square::C1, blockers)) << '\n';

    return 0;
}
