#pragma once
#include <unordered_map>
#include <vector>

#include "geometry.h"
#include "move_gen/attacks.h"
#include "position.h"
#include "search.h"
#include "types.h"
#include "ucioption.h"
#include "zobrist.h"

namespace engine {

constexpr std::string_view startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static std::once_flag initFlag;
inline void init_engine() noexcept {
    std::call_once(initFlag, []() {
        zobrist::init_zobrist();
        attacks::init_attack_tables();
        geom::init_geometry_tables();
    });
}

class Engine {
public:
    Engine();
    SearchResult search() { return search_.search(position_, searchLimits_); }

private:
    friend class UCIEngine;

    static constexpr int kDefaultHashMb = 256;
    static constexpr int kDefaultDepth = 8;
    static constexpr int kDefaultThreads = 1;

    std::vector<UCIOption> options_;
    Position position_{};
    TranspositionTable tt_{static_cast<size_t>(kDefaultHashMb)};
    SearchLimits searchLimits_{static_cast<uint8_t>(kDefaultDepth)};
    Search search_{&tt_};

    void setOption_(std::string name, std::string_view value);
    const UCIOption& option_(std::string_view name) const;
    void applyOption_(const UCIOption& option);
    void setPosition_(std::string_view fen);
    void debugSearch_();
};

}  // namespace engine
