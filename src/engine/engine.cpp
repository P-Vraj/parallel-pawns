#include "engine.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "uciengine.h"
#include "util.h"

namespace engine {

Engine::Engine() {
    options_ = { UCIOption::spin("Hash", kDefaultHashMb, 1, 65536),
                 UCIOption::spin("Default Depth", kDefaultDepth, 1, 255),
                 UCIOption::spin("Threads", kDefaultThreads, 1, 1024) };
    init_engine();
}

void Engine::setOption_(std::string name, std::string value) {
    auto option =
        std::ranges::find_if(options_, [&](const UCIOption& opt) { return opt.key == normalized_option_key(name); });
    if (option == options_.end()) {
        std::cout << "Unknown option: " << name << '\n';
        return;
    }

    if (!option->setValue(value)) {
        std::cout << "Invalid option or value: " << option->name << ": " << value << '\n';
        return;
    }

    applyOption_(*option);
}

const UCIOption& Engine::option_(std::string_view name) const {
    auto it =
        std::ranges::find_if(options_, [&](const UCIOption& opt) { return opt.key == normalized_option_key(name); });
    if (it == options_.end())
        throw std::invalid_argument("Unknown option: " + std::string(name));
    return *it;
}

void Engine::applyOption_(const UCIOption& option) {
    if (option.key == "hash")
        tt_.resize(static_cast<size_t>(option.getValue<int>()));
    else if (option.key == "default depth")
        searchLimits_.depth = static_cast<uint8_t>(option.getValue<int>());
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
