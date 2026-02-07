#include <iostream>
#include "types.h"
#include "util.h"
#include "move_gen_tables.h"

int main() {
    // constexpr auto knightAttacks = getKnightAttacks();
    std::cout << to_string(kKnightAttacks[0]) << '\n';

    // constexpr auto kingAttacks = getKingAttacks();
    std::cout << to_string(kKingAttacks[0]) << '\n';

    // constexpr auto whitePawnAttacks = getPawnAttacks<Color::White>();
    std::cout << to_string(kPawnAttacks[to_underlying(Color::White)][0]) << '\n';

    // constexpr auto blackPawnAttacks = getPawnAttacks<Color::Black>();
    std::cout << to_string(kPawnAttacks[to_underlying(Color::Black)][0]) << '\n';

    return 0;
}