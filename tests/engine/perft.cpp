#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string_view>
#include <vector>

#include "engine.h"
#include "move_gen/generator.h"
#include "position.h"
#include "search.h"

namespace {

// NOLINTBEGIN(misc-no-recursion)
uint64_t perft(Position& pos, Depth depth) {
    if (depth <= 0)
        return 1;

    const MoveList moves(pos);

    if (depth == 1)
        return static_cast<uint64_t>(moves.size());

    uint64_t nodes = 0;
    for (const Move m : moves) {
        UndoInfo u{};
        pos.makeMove(m, u);
        nodes += perft(pos, depth - 1);
        pos.undoMove(m, u);
    }

    return nodes;
}

uint64_t divide(Position& pos, Depth depth) {
    if (depth <= 0)
        return 1;

    const MoveList moves(pos);

    uint64_t nodes = 0;
    for (const Move m : moves) {
        UndoInfo u{};
        pos.makeMove(m, u);
        const uint64_t n = perft(pos, depth - 1);
        pos.undoMove(m, u);

        std::cout << to_string(m) << ": " << n << "\n";
        nodes += n;
    }
    std::cout << "\nNodes searched: " << nodes << "\n";

    return nodes;
}

void state_invariants(Position& pos, Depth depth) {
    if (depth <= 0)
        return;

    const MoveList moves(pos);

    for (const Move m : moves) {
        const std::string previousFEN = pos.toFEN();
        const Key previousHash = pos.hash();

        UndoInfo u{};
        pos.makeMove(m, u);

        REQUIRE(pos.hash() == pos.computeHash());

        state_invariants(pos, depth - 1);
        pos.undoMove(m, u);

        REQUIRE(pos.hash() == previousHash);
        REQUIRE(pos.toFEN() == previousFEN);
    }
}

void movegen_mode_invariants(Position& pos, Depth depth) {  // NOLINT(readability-function-cognitive-complexity)
    if (depth <= 0)
        return;

    const MoveList legalMoves(pos, GenMode::Legal);
    const MoveList tacticalMoves(pos, GenMode::Tactical);
    const bool inCheck = pos.inCheck();

    if (inCheck) {
        const MoveList evasions(pos, GenMode::Evasions);

        REQUIRE(legalMoves.size() == evasions.size());
        REQUIRE(legalMoves.size() == tacticalMoves.size());

        for (const Move m : legalMoves) {
            CHECK(evasions.contains(m));
            CHECK(tacticalMoves.contains(m));
        }
    }
    else {
        size_t tacticalCount = 0;

        for (const Move m : legalMoves) {
            if (m.isCapture() || m.isPromotion()) {
                ++tacticalCount;
                CHECK(tacticalMoves.contains(m));
            }
        }

        for (const Move m : tacticalMoves) {
            CHECK((m.isCapture() || m.isPromotion()));
            CHECK(legalMoves.contains(m));
        }

        CHECK(tacticalCount == tacticalMoves.size());
    }

    for (const Move m : legalMoves) {
        UndoInfo u{};
        pos.makeMove(m, u);
        movegen_mode_invariants(pos, depth - 1);
        pos.undoMove(m, u);
    }
}
// NOLINTEND(misc-no-recursion)

std::vector<Key> build_history(Position& pos, std::string_view movesUci) {
    std::vector<Key> history{pos.hash()};
    std::istringstream iss{std::string(movesUci)};
    for (std::string uciMove; iss >> uciMove;) {
        MoveList moves(pos);
        auto* it = std::ranges::find_if(moves, [&](Move m) { return to_string(m) == uciMove; });
        REQUIRE(it != moves.end());

        const bool irreversible =
            it->isCapture() || it->isPromotion() || piece_type(pos.pieceOn(it->from())) == PieceType::Pawn;
        UndoInfo u{};
        pos.makeMove(*it, u);
        if (irreversible)
            history.clear();
        history.push_back(pos.hash());
    }
    return history;
}

}  // namespace

struct PerftCase {
    const char* name;
    const char* fen;
    std::vector<std::pair<int, uint64_t>> checks;  // (depth, expected nodes)
};

// Tests that the number of nodes at each depth matches known values for different positions
TEST_CASE("Perft", "[perft][movegen]") {
    engine::init_engine();

    // Test cases referenced from the Chess Programming Wiki page on perft
    // Read more here: https://www.chessprogramming.org/Perft_Results
    // clang-format off
    const std::vector<PerftCase> cases = {
        {
            "Position 1 (Startpos)",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            {
                { 1, 20 },
                { 2, 400 },
                { 3, 8902 },
                { 4, 197281 },
                { 5, 4865609 },
                { 6, 119060324 },
            },
        },
        {
            "Position 2 (Kiwipete)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            {
                { 1, 48 },
                { 2, 2039 },
                { 3, 97862 },
                { 4, 4085603 },
                { 5, 193690690 },
            },
        },
        {
            "Position 3",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            {
                { 1, 14 },
                { 2, 191 },
                { 3, 2812 },
                { 4, 43238 },
                { 5, 674624 },
                { 6, 11030083 },
            },
        },
        {
            "Position 4",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            {
                { 1, 6 },
                { 2, 264 },
                { 3, 9467 },
                { 4, 422333 },
                { 5, 15833292 },
            },
        },
        {
            "Position 5",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            {
                { 1, 44 },
                { 2, 1486 },
                { 3, 62379 },
                { 4, 2103487 },
                { 5, 89941194 },
          }
        },
        {
            "Position 6",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
            {
                { 1, 46 },
                { 2, 2079 },
                { 3, 89890 },
                { 4, 3894594 },
                { 5, 164075551 },
            }
        },
    };
    // clang-format on

    for (const auto& tc : cases) {
        for (auto [depth, expected] : tc.checks) {
            Position pos = Position::fromFEN(tc.fen);
            const uint64_t got = perft(pos, depth);

            INFO(tc.name << " depth=" << depth);
            CHECK(got == expected);
            if (got != expected) {
                divide(pos, depth);
            }
        }
    }
}

struct InvariantCase {
    const char* name;
    const char* fen;
    Depth depth;
};

// Tests that the position invariants hold at each node of the search tree up to the given depth.
// Tests that the rolling hashes and FEN representations are always consistent.
TEST_CASE("State Invariants", "[state][invariants]") {
    engine::init_engine();

    // clang-format off
    const std::vector<InvariantCase> cases = {
        {
            "Position 1 (Startpos)",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            4,
        },
        {
            "Position 2 (Kiwipete)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            4,
        },
        {
            "Position 3",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            4,
        },
        {
            "Position 4",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            4,
        },
        {
            "Position 5",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            4,
        },
        {
            "Position 6",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
            4,
        },
    };
    // clang-format on

    for (const auto& tc : cases) {
        Position pos = Position::fromFEN(tc.fen);
        state_invariants(pos, tc.depth);
    }
}

// Tests that all the different modes of move generation are consistent with each other.
TEST_CASE("Move Generation Mode Invariants", "[movegen][invariants]") {
    engine::init_engine();

    // clang-format off
    const std::vector<InvariantCase> cases = {
        {
            "Position 1 (Startpos)",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            4,
        },
        {
            "Position 2 (Kiwipete)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            4,
        },
        {
            "Position 3",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            4,
        },
        {
            "Position 4",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            4,
        },
        {
            "Position 5",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            4,
        },
        {
            "Position 6",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
            4,
        },
    };
    // clang-format on

    for (const auto& tc : cases) {
        Position pos = Position::fromFEN(tc.fen);
        movegen_mode_invariants(pos, tc.depth);
    }
}

TEST_CASE("Search Detects Draw Rules", "[search][draw]") {
    engine::init_engine();

    SECTION("Threefold repetition at root") {
        Position pos = Position::fromFEN(engine::startpos);
        const auto history = build_history(pos, "g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8");

        engine::Search search(nullptr, nullptr, 0, history);
        const engine::SearchResult result = search.search(pos, engine::SearchLimits{.depth = 4});

        CHECK(result.score == kDrawScore);
        CHECK(result.bestMove.isNone());
    }

    SECTION("Fifty-move rule at root") {
        Position pos = Position::fromFEN("8/8/8/8/8/8/8/K6k w - - 100 1");

        engine::Search search(nullptr);
        const engine::SearchResult result = search.search(pos, engine::SearchLimits{.depth = 4});

        CHECK(result.score == kDrawScore);
        CHECK(result.bestMove.isNone());
    }
}
