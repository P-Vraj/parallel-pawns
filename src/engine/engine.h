#pragma once
#include <thread>
#include <vector>

#include "geometry.h"
#include "move_gen/attacks.h"
#include "position.h"
#include "search.h"
#include "types.h"
#include "ucioption.h"
#include "zobrist.h"

namespace engine {

inline constexpr std::string_view startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static std::once_flag initFlag;
inline void init_engine() noexcept {
    std::call_once(initFlag, []() {
        zobrist::init_zobrist();
        attacks::init_attack_tables();
        geom::init_geometry_tables();

        if (const std::atomic<PackedTTEntry> tableEntry{}; !tableEntry.is_lock_free()) {
            std::cerr << "Lock-free atomic transposition table entries are not available on this platform!\n";
            std::exit(EXIT_FAILURE);
        }
    });
}

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

private:
    friend class UCIEngine;

    static constexpr int kDefaultHashMb = 256;
    static constexpr int kDefaultDepth = 8;
    static constexpr int kDefaultThreads = 1;

    std::vector<UCIOption> options_;
    Position position_{};
    std::vector<Key> positionHistory_{};
    TranspositionTable tt_{static_cast<size_t>(kDefaultHashMb)};
    SearchLimits searchLimits_{kDefaultDepth, kDefaultThreads};
    SearchSharedState sharedSearchState_{};
    std::thread searchThread_;

    void setOption_(std::string name, std::string_view value);
    const UCIOption& option_(std::string_view name) const;
    void applyOption_(const UCIOption& option);
    void setPosition_(std::string_view fen);
    void recordCurrentPosition_(bool irreversible);
    static bool isIrreversibleMove_(const Position& pos, Move move) noexcept;
    void startSearch_(
        std::optional<Depth> depthOverride,
        bool infinite,
        std::optional<std::chrono::milliseconds> moveTime,
        std::optional<SearchLimits::TimeControl> timeControl
    );
    void stopSearch_();
    SearchResult runSearch_(const Position& root, const SearchLimits& limits);
    static void mergeSearchResult_(SearchResult& aggregate, const SearchResult& workerResult, bool preferWorker);
    void printSearchResult_(
        const Position& root,
        const SearchLimits& limits,
        const SearchResult& result,
        uint64_t elapsedMs
    );
};

}  // namespace engine
