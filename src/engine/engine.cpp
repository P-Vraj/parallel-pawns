#include <iostream>

#include "geometry.h"
#include "move_gen/attacks.h"
#include "position.h"
#include "types.h"
#include "util.h"
#include "zobrist.h"

using namespace attacks;

void engine_init() {
    zobrist::init();
    attacks::init_attack_tables();
    geom::init_geometry_tables();
}

int main() {
    engine_init();

    Bitboard blockers = bitboard(Square::B2) | bitboard(Square::F1);
    std::cout << "Blockers:\n" << to_string(blockers) << '\n';
    std::cout << "King attacks:\n" << to_string(king_attacks(Square::D4)) << '\n';
    std::cout << "White pawn attacks:\n" << to_string(pawn_attacks(Color::White, Square::D4)) << '\n';
    std::cout << "Black pawn attacks:\n" << to_string(pawn_attacks(Color::Black, Square::D4)) << '\n';
    std::cout << "Knight attacks:\n" << to_string(knight_attacks(Square::D4)) << '\n';
    std::cout << "Bishop attacks:\n" << to_string(bishop_attacks(Square::C1, blockers)) << '\n';
    std::cout << "Rook attacks:\n" << to_string(rook_attacks(Square::A1, blockers)) << '\n';
    std::cout << "Queen attacks:\n" << to_string(queen_attacks(Square::C1, blockers)) << '\n';

    std::string startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::string testFEN = "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b Kkq - 1 2";

    Position pos = Position::fromFEN(startFEN);
    std::cout << "Position:\n";
    std::cout << to_string(pos) << '\n';
    std::cout << "FEN: " << pos.toFEN() << '\n';

    Position pos2 = Position::fromFEN(testFEN);
    if (pos2.toFEN() == testFEN) {
        std::cout << "FEN round-trip successful!\n";
    }

    return 0;
}
