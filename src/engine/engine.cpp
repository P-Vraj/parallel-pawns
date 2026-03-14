#include "engine.h"

#include <chrono>
#include <iostream>

#include "search.h"

engine::SearchResult search_test(std::string_view fen, int depth) {
    Position pos = Position::fromFEN(fen);
    if (pos.toFEN() != fen)
        std::cout << "\nFEN round-trip failed!\n";

    std::cout << "\nStarting search for: " << fen << " at depth " << depth << "\n";

    TranspositionTable tt(64);
    engine::Search search(&tt);
    const engine::SearchLimits limits{ static_cast<uint8_t>(depth) };

    auto start = std::chrono::high_resolution_clock::now();
    const engine::SearchResult result = search.search(pos, limits);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Search result for: " << fen << '\n';
    std::cout << "Best move: " << to_string(result.bestMove) << '\n';
    std::cout << "Score (relative): " << result.score << '\n';
    std::cout << "Score (absolute): " << engine::absolute_eval(result.score, pos.sideToMove()) << '\n';
    std::cout << "Nodes searched: " << result.nodes << '\n';
    std::cout << "Quiescence nodes searched: " << result.qNodes << '\n';
    std::cout << "Time taken: " << elapsed.count() << " seconds\n";
    std::cout << "Total nodes per second: " << (result.nodes + result.qNodes) / elapsed.count() << '\n';
    std::cout << "TT hits: " << tt.hits() << '\n';
    std::cout << "TT misses: " << tt.misses() << '\n';
    std::cout << "TT hit rate: " << tt.hitRate() * 100.0f << "%\n";
    
    return result;
}

void bitboards_test() {
    const Bitboard blockers = bitboard(Square::B2) | bitboard(Square::F1);
    std::cout << "Blockers:\n" << to_string(blockers) << '\n';
    std::cout << "King attacks:\n" << to_string(attacks::king_attacks(Square::D4)) << '\n';
    std::cout << "White pawn attacks:\n" << to_string(attacks::pawn_attacks(Color::White, Square::D4)) << '\n';
    std::cout << "Black pawn attacks:\n" << to_string(attacks::pawn_attacks(Color::Black, Square::D4)) << '\n';
    std::cout << "Knight attacks:\n" << to_string(attacks::knight_attacks(Square::D4)) << '\n';
    std::cout << "Bishop attacks:\n" << to_string(attacks::bishop_attacks(Square::C1, blockers)) << '\n';
    std::cout << "Rook attacks:\n" << to_string(attacks::rook_attacks(Square::A1, blockers)) << '\n';
    std::cout << "Queen attacks:\n" << to_string(attacks::queen_attacks(Square::C1, blockers)) << '\n';
}

void fen_round_trip_test(std::string_view fen) {
    const Position pos = Position::fromFEN(fen);
    std::cout << "\nPosition:\n" << to_string(pos) << '\n';
    std::cout << "FEN: " << pos.toFEN() << '\n';

    if (pos.toFEN() == fen)
        std::cout << "FEN round-trip successful!\n";
    else
        std::cout << "FEN round-trip failed!\n";
}

void interactive_test(std::string_view fen, int depth) {
    Position pos = Position::fromFEN(fen);
    std::string input;

    // Starts on the opponent's move as input
    while (true) {
        std::cin >> input;
        if (input == "exit")
            break;

        MoveList moves(pos);
        UndoInfo u{};
        // Opponent move
        auto it = std::find_if(moves.begin(), moves.end(), [&](Move m) { return to_string(m) == input; });
        if (it != moves.end()) {
            pos.makeMove(*it, u);
            std::cout << "Opponent made move: " << to_string(*it) << '\n';
        }
        else {
            std::cout << "Invalid move. Please try again.\n";
            continue;
        }
        // Engine move
        const auto result = search_test(pos.toFEN(), depth);
        pos.makeMove(result.bestMove, u);
        std::cout << "Engine made move: " << to_string(result.bestMove) << '\n';
    }
}

int main() {
    engine::init_engine();

    const std::string startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const std::string testFEN = "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1P1P/RNBQKB1R b Kkq - 1 2";  // Down 1 Pawn

    // bitboards_test();
    // fen_round_trip_test(startFEN);
    // fen_round_trip_test(testFEN);

    // search_test(startFEN, 6);
    // search_test(testFEN, 6);
    // search_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5);
    // search_test("r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1", 9);

    // search_test("rnbqkb1r/4pppp/2p2n2/pp2P3/2pP4/2N2N2/PP3PPP/R1BQKB1R w KQkq a6 0 7", 8);

    interactive_test("rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP1BPPPP/R2QKBNR b KQkq - 3 4", 8);

    return 0;
}