#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <vector>

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
    bool iterativeDeepening{true};
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

struct SearchTelemetry {
    uint64_t nodes{};           // Number of regular search nodes searched
    uint64_t qNodes{};          // Number of quiescence nodes searched
    uint64_t elapsedMs{};       // Wall-clock time for the completed search
    uint64_t ttHits{};          // TT probe hits across all workers
    uint64_t ttMisses{};        // TT probe misses across all workers
    uint64_t ttWrites{};        // TT writes into empty slots
    uint64_t ttRewrites{};      // TT rewrites of existing slots
    Depth completedDepth{};     // Deepest fully completed iterative deepening iteration
};

struct SearchResult {
    Eval score{};                    // Relative evaluation of the position
    Move bestMove;                   // Best move found in the search, or `Move::none()` if no move found
    std::array<Move, kMaxPly> pv{};  // Principal variation from the root
    uint8_t pvLength{};              // Number of moves in `pv`
    bool stopped{};                  // Whether the search stopped before reaching its target depth
    SearchTelemetry telemetry{};     // Search counters and timing info for this completed search
};

using SearchIterationCallback = std::function<void(const SearchResult&)>;

struct SearchSharedState {
    std::atomic<bool> stopRequested{false};
    std::optional<std::chrono::steady_clock::time_point> softDeadline;
    std::optional<std::chrono::steady_clock::time_point> hardDeadline;
};

class Search {
public:
    Search() = default;
    explicit Search(
        TranspositionTable* tt,
        SearchSharedState* sharedState = nullptr,
        int workerId = 0,
        std::vector<Key> rootHistory = {}
    ) noexcept
        : tt_(tt), sharedState_(sharedState), workerId_(workerId), positionHistory_(std::move(rootHistory)) {}

    SearchResult search(Position& pos, const SearchLimits& limits);
    SearchResult search(Position& pos, const SearchLimits& limits, std::span<const Move> rootMoves);
    void setIterationCallback(SearchIterationCallback callback) { iterationCallback_ = std::move(callback); }

private:
    SearchResult searchImpl_(Position& pos, const SearchLimits& limits, std::span<const Move> rootMoves);
    Eval alphaBeta_(Position& pos, Depth depth, Eval alpha, Eval beta, int ply);
    Eval quiescence_(Position& pos, Eval alpha, Eval beta, int ply);
    static Eval evaluate_(const Position& pos) noexcept;
    bool isTerminal_(const Position& pos, const MoveList& moves, int ply, Eval& terminalScore) const noexcept;
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
    // Diversifies the move ordering of root moves based on the worker ID for Lazy SMP.
    void diversifyRootMoves_(MoveList& moves) const noexcept;
    bool isDrawByRepetition_(const Position& pos) const noexcept;
    static bool isIrreversibleMove_(const Position& pos, Move move) noexcept;
    void pushHistory_(const Position& pos, bool irreversible);
    void popHistory_() noexcept;

    TranspositionTable* tt_{nullptr};
    SearchSharedState* sharedState_{nullptr};
    int workerId_{};
    uint64_t nodes_{};
    uint64_t qNodes_{};
    bool aborted_{false};

    // Principal variation table updated during search
    std::array<std::array<Move, kMaxPly>, kMaxPly> pvTable_{};
    std::array<uint8_t, kMaxPly> pvLength_{};

    // Heuristic move ordering
    std::array<std::array<Move, 2>, kMaxPly> killers_{};
    std::array<std::array<std::array<int, 64>, 64>, to_underlying(Color::Count)> history_{};

    // History of position hashes (since the last irreversible move) for repetition detection
    std::vector<Key> positionHistory_;
    std::vector<size_t> irreversibleHistoryStarts_;
    size_t irreversibleHistoryStart_{0};
    SearchIterationCallback iterationCallback_{};
};

}  // namespace engine
