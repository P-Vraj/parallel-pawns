#include "engine.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "uciengine.h"
#include "util.h"

namespace engine {

namespace {

int uciMateDistance(Eval score) noexcept {
    const int matePly = kMateScore - std::abs(score);
    const int mateMoves = (matePly + 1) / 2;
    return score > 0 ? mateMoves : -mateMoves;
}

void printUciScore(Eval score) {
    if (is_mate_score(score))
        std::cout << "score mate " << uciMateDistance(score);
    else
        std::cout << "score cp " << score;
}

void printUciPV(const SearchResult& result) {
    if (result.pvLength == 0)
        return;

    std::cout << " pv";
    for (uint8_t i = 0; i < result.pvLength; ++i) {
        std::cout << ' ' << to_string(result.pv[i]);
    }
}

}  // namespace

Engine::Engine() {
    options_ = {
        UCIOption::spin("Hash", kDefaultHashMb, 1, 65536),
        UCIOption::spin("Default Depth", kDefaultDepth, 1, 255),
        UCIOption::spin("Threads", kDefaultThreads, 1, 1024)
    };
    init_engine();
}

void Engine::setOption_(std::string name, std::string_view value) {
    auto option =
        std::ranges::find_if(options_, [&](const UCIOption& opt) { return opt.key() == normalized_option_key(name); });
    if (option == options_.end()) {
        std::cout << "Unknown option: " << name << '\n';
        return;
    }

    if (!option->setValue(value)) {
        std::cout << "Invalid value: " << value << '\n';
        return;
    }

    applyOption_(*option);
}

const UCIOption& Engine::option_(std::string_view name) const {
    auto it =
        std::ranges::find_if(options_, [&](const UCIOption& opt) { return opt.key() == normalized_option_key(name); });
    if (it == options_.end())
        throw std::invalid_argument("Unknown option: " + std::string(name));
    return *it;
}

void Engine::applyOption_(const UCIOption& option) {
    if (option.key() == "hash")
        tt_.resize(static_cast<size_t>(option.getValue<int>()));
    else if (option.key() == "default depth")
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
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    const uint64_t totalNodes = result.nodes + result.qNodes;
    const auto nps = static_cast<uint64_t>((1000 * totalNodes) / elapsedMs);

    std::cout << "info\n";
    std::cout << "Search result for: " << position_.toFEN() << '\n';
    std::cout << "Score (Relative): " << result.score << '\n';
    std::cout << "Score (Absolute): " << engine::absolute_eval(result.score, position_.sideToMove()) << '\n';
    std::cout << "Nodes searched (Search): " << result.nodes << '\n';
    std::cout << "Nodes searched (Quiescence): " << result.qNodes << '\n';
    std::cout << "Time taken: " << (static_cast<double>(elapsedMs) / 1000.0F) << " seconds\n";
    std::cout << "NPS (Total): " << nps << '\n';
    std::cout << "TT size: " << (tt_.size() * sizeof(TTEntry)) / 1024 / 1024 << " MB\n";
    std::cout << "TT hits: " << tt_.hits() << '\n';
    std::cout << "TT misses: " << tt_.misses() << '\n';
    std::cout << "TT hit rate: " << tt_.hitRate() * 100.0F << "%\n";
    std::cout << "TT writes: " << tt_.writes() << '\n';
    std::cout << "TT rewrites: " << tt_.rewrites() << '\n';
    std::cout << "TT rewrite rate: " << tt_.rewriteRate() * 100.0F << "%\n";
    std::cout << "info depth " << searchLimits_.depth << ' ';
    printUciScore(result.score);
    std::cout << " nodes " << totalNodes << " time " << elapsedMs << " nps " << nps;
    printUciPV(result);
    std::cout << '\n';
    std::cout << to_string(position_) << '\n';
    std::cout << "bestmove " << to_string(result.bestMove) << '\n';
}

}  // namespace engine
