#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <optional>

#include "eval_constants.h"
#include "move_gen/generator.h"
#include "position.h"
#include "transposition_table.h"
#include "types.h"

namespace engine {

// Converts an eval from a relative perspective to an absolute perspective.
constexpr Eval absolute_eval(Eval score, Color sideToMove) noexcept {
    return (sideToMove == Color::White) ? score : -score;
}

struct SearchLimits {
    Depth depth{1};
    int threads{1};
    bool infinite{false};
    std::optional<std::chrono::milliseconds> moveTime{};

    struct TimeControl {
        std::optional<std::chrono::milliseconds> whiteTime;
        std::optional<std::chrono::milliseconds> blackTime;
        std::optional<std::chrono::milliseconds> whiteIncrement;
        std::optional<std::chrono::milliseconds> blackIncrement;
        std::optional<int> movesToGo;
    };

    std::optional<TimeControl> timeControl{};
};

struct SearchResult {
    uint64_t nodes{};                // Number of nodes searched
    uint64_t qNodes{};               // Number of quiescence nodes searched
    Eval score{};                    // Relative evaluation of the position
    Depth completedDepth{};          // Deepest fully completed iterative deepening iteration
    Move bestMove;                   // Best move found in the search, or `Move::none()` if no move found
    std::array<Move, kMaxPly> pv{};  // Principal variation from the root
    uint8_t pvLength{};              // Number of moves in `pv`
    bool stopped{};                  // Whether the search stopped before reaching its target depth
};

struct SearchSharedState {
    std::atomic<bool> stopRequested{false};
    std::optional<std::chrono::steady_clock::time_point> softDeadline;
    std::optional<std::chrono::steady_clock::time_point> hardDeadline;
};

class Search {
public:
    Search() = default;
    explicit Search(TranspositionTable* tt, SearchSharedState* sharedState = nullptr, int workerId = 0) noexcept
        : tt_(tt), sharedState_(sharedState), workerId_(workerId) {}

    SearchResult search(Position& pos, const SearchLimits& limits);

private:
    Eval alphaBeta_(Position& pos, Depth depth, Eval alpha, Eval beta, int ply);
    Eval quiescence_(Position& pos, Eval alpha, Eval beta, int ply);
    static Eval evaluate_(const Position& pos) noexcept;
    static bool isTerminal_(const Position& pos, const MoveList& moves, int ply, Eval& terminalScore) noexcept;
    void resetHeuristics_() noexcept;
    void orderMoves_(const Position& pos, MoveList& moves, Move ttMove, int ply) const noexcept;
    int scoreMove_(const Position& pos, Move move, Move ttMove, int ply) const noexcept;
    void updateQuietHeuristics_(const Position& pos, Move move, int ply, Depth depth) noexcept;
    static bool isBadQCapture_(const Position& pos, Move move) noexcept;
    static int scoreQMove_(const Position& pos, Move move) noexcept;
    static int scoreQEvasion_(const Position& pos, Move move) noexcept;
    static void orderQMoves_(const Position& pos, MoveList& moves, bool inCheck) noexcept;
    bool shouldStopHard_() noexcept;
    bool shouldStopSoft_() const noexcept;
    void diversifyRootMoves_(MoveList& moves) const noexcept;

    TranspositionTable* tt_{nullptr};
    SearchSharedState* sharedState_{nullptr};
    int workerId_{};
    uint64_t nodes_{};
    uint64_t qNodes_{};
    bool aborted_{false};
    std::array<std::array<Move, kMaxPly>, kMaxPly> pvTable_{};
    std::array<uint8_t, kMaxPly> pvLength_{};
    std::array<std::array<Move, 2>, kMaxPly> killers_{};
    std::array<std::array<std::array<int, 64>, 64>, to_underlying(Color::Count)> history_{};
};

}  // namespace engine
