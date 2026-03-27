#pragma once
#include <unordered_map>

#include "geometry.h"
#include "move_gen/attacks.h"
#include "position.h"
#include "search.h"
#include "types.h"
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
    explicit Engine() noexcept { init_engine(); }
    SearchResult search() noexcept { return search_.search(position_, searchLimits_); }

private:
    friend class UCIEngine;

    std::unordered_map<std::string, Option> options_{
        { "hash", 256 },         // Default TT size in MB
        { "default depth", 8 },  // Default search depth
        { "threads", 1 },        // Number of threads to use
    };
    Position position_{};
    TranspositionTable tt_{ static_cast<size_t>(std::get<int>(options_["hash"])) };
    SearchLimits searchLimits_{ static_cast<uint8_t>(std::get<int>(options_["default depth"])) };
    Search search_{ &tt_ };

    void setOption_(std::string name, Option value) noexcept;
    void setPosition_(std::string_view fen) noexcept;
    void debugSearch_() noexcept;
};

}  // namespace engine