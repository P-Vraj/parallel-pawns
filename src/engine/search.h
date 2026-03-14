#pragma once

#include <optional>

#include "move_gen/generator.h"
#include "position.h"
#include "transposition_table.h"
#include "types.h"

namespace engine {

// Converts an eval from a relative perspective to an absolute perspective.
constexpr Eval absolute_eval(Eval score, Color sideToMove) noexcept {
    return (sideToMove == Color::White) ? score : static_cast<Eval>(-score);
}

struct SearchLimits {
    uint8_t depth{ 1 };
};

struct SearchResult {
    uint64_t nodes{};   // Number of nodes searched
    uint64_t qNodes{};  // Number of quiescence nodes searched
    Eval score{};       // Relative evaluation of the position
    Move bestMove{};    // Best move found in the search, or `Move::none()` if no move found
};
static_assert(sizeof(SearchResult) == 24);

class Search {
public:
    Search() = default;
    explicit Search(TranspositionTable* tt) noexcept : tt_(tt) {}

    SearchResult search(Position& pos, const SearchLimits& limits);

private:
    Eval alphaBeta_(Position& pos, int depth, Eval alpha, Eval beta, int ply);
    Eval quiescence_(Position& pos, Eval alpha, Eval beta, int ply);
    Eval evaluate_(const Position& pos) const noexcept;
    bool isTerminal_(const Position& pos, const MoveList& moves, int ply, Eval& terminalScore) const noexcept;
    void resetHeuristics_() noexcept;
    void orderMoves_(const Position& pos, MoveList& moves, Move ttMove, int ply) const noexcept;
    int scoreMove_(const Position& pos, Move move, Move ttMove, int ply) const noexcept;
    void updateQuietHeuristics_(const Position& pos, Move move, int ply, int depth) noexcept;
    bool isBadQCapture_(const Position& pos, Move move) const noexcept;
    int scoreQMove_(const Position& pos, Move move) const noexcept;
    int scoreQEvasion_(const Position& pos, Move move) const noexcept;
    void orderQMoves_(const Position& pos, MoveList& moves, bool inCheck) const noexcept;

private:
    TranspositionTable* tt_{ nullptr };
    uint64_t nodes_{};
    uint64_t qNodes_{};
    std::array<std::array<Move, 2>, kMaxPly> killers_{};
    std::array<std::array<std::array<int, 64>, 64>, to_underlying(Color::Count)> history_{};
};

}  // namespace engine
