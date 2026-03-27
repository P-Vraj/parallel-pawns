#include "engine.h"

#include <iostream>

#include "uciengine.h"

namespace engine {

void Engine::setOption_(std::string name, Option value) {
    std::ranges::transform(name, name.begin(), ::tolower);

    if (!options_.contains(name)) {
        std::cout << "Unknown option: " << name << '\n';
        return;
    }

    options_[name] = value;
    if (name == "hash") {
        if (const auto* size = std::get_if<int>(&value)) {
            tt_.resize(static_cast<size_t>(*size));
        }
    }
    else if (name == "default depth") {
        if (const auto* depth = std::get_if<int>(&value)) {
            searchLimits_.depth = static_cast<uint8_t>(*depth);
        }
    }
}

void Engine::setPosition_(std::string_view fen) {
    position_ = Position::fromFEN(fen);
    tt_.clear();
}

void Engine::debugSearch_() {
    auto start = std::chrono::high_resolution_clock::now();
    const auto result = search();
    auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> elapsed = end - start;

    std::cout << "info\n";
    std::cout << "Search result for: " << position_.toFEN() << '\n';
    std::cout << "Score (Relative): " << result.score << '\n';
    std::cout << "Score (Absolute): " << engine::absolute_eval(result.score, position_.sideToMove()) << '\n';
    std::cout << "Nodes searched (Search): " << result.nodes << '\n';
    std::cout << "Nodes searched (Quiescence): " << result.qNodes << '\n';
    std::cout << "Time taken: " << elapsed.count() << " seconds\n";
    std::cout << "NPS (Total): " << static_cast<double>((result.nodes + result.qNodes)) / elapsed.count() << '\n';
    std::cout << "TT size: " << (tt_.size() * sizeof(TTEntry)) / 1024 / 1024 << " MB\n";
    std::cout << "TT hits: " << tt_.hits() << '\n';
    std::cout << "TT misses: " << tt_.misses() << '\n';
    std::cout << "TT hit rate: " << tt_.hitRate() * 100.0F << "%\n";
    std::cout << "TT writes: " << tt_.writes() << '\n';
    std::cout << "TT rewrites: " << tt_.rewrites() << '\n';
    std::cout << "TT rewrite rate: " << tt_.rewriteRate() * 100.0F << "%\n";
    std::cout << to_string(position_) << '\n';
    std::cout << "bestmove " << to_string(result.bestMove) << '\n';
}

}  // namespace engine