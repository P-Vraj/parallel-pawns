#include "engine.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <utility>

#include "uciengine.h"
#include "util.h"

namespace engine {

namespace {

struct TimeBudget {
    std::optional<std::chrono::milliseconds> softLimit;
    std::optional<std::chrono::milliseconds> hardLimit;
};

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

std::string summarizeRootMoves(const std::vector<Move>& moves) {
    std::string out;
    for (size_t i = 0; i < moves.size(); ++i) {
        if (i != 0)
            out += ',';
        out += to_string(moves[i]);
    }
    return out;
}

TimeBudget buildTimeBudget(const SearchLimits& limits, Color sideToMove) noexcept {
    if (limits.moveTime.has_value())
        return TimeBudget{.softLimit = limits.moveTime, .hardLimit = limits.moveTime};

    if (!limits.timeControl.has_value())
        return {};

    const SearchLimits::TimeControl& timeControl = *limits.timeControl;
    const auto remaining = sideToMove == Color::White ? timeControl.whiteTime : timeControl.blackTime;
    const auto increment = sideToMove == Color::White ? timeControl.whiteIncrement : timeControl.blackIncrement;
    if (!remaining.has_value())
        return {};

    constexpr int64_t kMoveOverheadMs = 15;
    constexpr int64_t kMinSearchTimeMs = 10;
    const int64_t remainingMs = remaining->count();
    const int64_t usableMs = std::max<int64_t>(1, remainingMs - kMoveOverheadMs);
    const int movesLeft = std::max(1, timeControl.movesToGo.value_or(25));
    const int64_t incrementMs = increment.value_or(std::chrono::milliseconds{0}).count();

    const int64_t baseMs = usableMs / movesLeft;
    const int64_t targetMs = baseMs + (incrementMs / 2);
    const int64_t softCapMs = std::max<int64_t>(kMinSearchTimeMs, usableMs / 8);
    const int64_t softMs = std::clamp<int64_t>(targetMs, kMinSearchTimeMs, softCapMs);
    const int64_t hardCapMs = std::max<int64_t>(softMs, usableMs / 2);
    const int64_t hardMs = std::clamp<int64_t>(std::max<int64_t>(softMs + 10, (softMs * 3) / 2), softMs, hardCapMs);

    return TimeBudget{.softLimit = std::chrono::milliseconds{softMs}, .hardLimit = std::chrono::milliseconds{hardMs}};
}

bool shouldUseDistributedSearch(
    const SearchLimits& limits,
    const SearchSharedState& sharedState,
    const std::vector<DistributedWorkerEndpoint>& distributedWorkers
) noexcept {
    if (distributedWorkers.empty() || limits.infinite)
        return false;

    constexpr auto kMinDistributedMovetime = std::chrono::milliseconds{750};
    constexpr auto kMinDistributedSoftWindow = std::chrono::milliseconds{500};

    if (limits.moveTime.has_value() && *limits.moveTime < kMinDistributedMovetime)
        return false;

    const auto now = std::chrono::steady_clock::now();
    if (sharedState.hardDeadline.has_value() && *sharedState.hardDeadline <= now + kMinDistributedMovetime)
        return false;
    if (sharedState.softDeadline.has_value() && *sharedState.softDeadline <= now + kMinDistributedSoftWindow)
        return false;

    return true;
}

}  // namespace

Engine::Engine() {
    options_ = {
        UCIOption::spin("Hash", kDefaultHashMb, 1, 65536),
        UCIOption::spin("Default Depth", kDefaultDepth, 1, 255),
        UCIOption::spin("Threads", kDefaultThreads, 1, 1024),
        UCIOption::string("Distributed_Workers", "")
    };
    init_engine();
    position_ = Position::fromFEN(startpos);
    positionHistory_.push_back(position_.hash());
}

Engine::~Engine() {
    stopSearch_();
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
    stopSearch_();

    if (option.key() == "hash")
        tt_.resize(static_cast<size_t>(option.getValue<int>()));
    else if (option.key() == "default depth")
        searchLimits_.depth = static_cast<uint8_t>(option.getValue<int>());
    else if (option.key() == "threads")
        searchLimits_.threads = option.getValue<int>();
    else if (option.key() == "distributed workers") {
        std::vector<DistributedWorkerEndpoint> endpoints;
        std::string error;
        if (!parseDistributedWorkerEndpoints(option.getValue<std::string>(), endpoints, error)) {
            std::cout << "info string Invalid Distributed Workers value: " << error << '\n';
            std::cout.flush();
            return;
        }
        distributedWorkers_ = std::move(endpoints);
    }

    if (option.key() == "hash")
        tt_.clear();
}

void Engine::setPosition_(std::string_view fen) {
    stopSearch_();
    position_ = Position::fromFEN(fen);
    positionHistory_.clear();
    positionHistory_.push_back(position_.hash());
}

void Engine::recordCurrentPosition_(bool irreversible) {
    if (irreversible)
        positionHistory_.clear();
    positionHistory_.push_back(position_.hash());
}

bool Engine::isIrreversibleMove_(const Position& pos, Move move) noexcept {
    return move.isCapture() || move.isPromotion() || piece_type(pos.pieceOn(move.from())) == PieceType::Pawn;
}

void Engine::startSearch_(
    std::optional<Depth> depthOverride,
    bool infinite,
    std::optional<std::chrono::milliseconds> moveTime,
    std::optional<SearchLimits::TimeControl> timeControl
) {
    stopSearch_();

    const SearchLimits limits{
        .depth = depthOverride.value_or(searchLimits_.depth),
        .threads = searchLimits_.threads,
        .infinite = infinite,
        .moveTime = moveTime,
        .timeControl = timeControl
    };

    const Position root = position_;
    tt_.resetCounters();
    tt_.newSearch();
    const TimeBudget timeBudget = buildTimeBudget(limits, root.sideToMove());

    sharedSearchState_.stopRequested.store(false, std::memory_order_relaxed);
    if (timeBudget.softLimit.has_value())
        sharedSearchState_.softDeadline = std::chrono::steady_clock::now() + *timeBudget.softLimit;
    else
        sharedSearchState_.softDeadline.reset();
    if (timeBudget.hardLimit.has_value())
        sharedSearchState_.hardDeadline = std::chrono::steady_clock::now() + *timeBudget.hardLimit;
    else
        sharedSearchState_.hardDeadline.reset();

    searchThread_ = std::thread([this, root = root, limits]() mutable {
        const auto start = std::chrono::steady_clock::now();
        SearchResult result = runSearch_(root, limits);
        const auto end = std::chrono::steady_clock::now();
        const auto elapsedMs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        const TTStats ttStats = tt_.stats();

        result.telemetry.elapsedMs = elapsedMs;
        result.telemetry.ttHits = ttStats.hits;
        result.telemetry.ttMisses = ttStats.misses;
        result.telemetry.ttWrites = ttStats.writes;
        result.telemetry.ttRewrites = ttStats.rewrites;

        printSearchResult_(limits, result, elapsedMs);
    });
}

void Engine::stopSearch_() {
    sharedSearchState_.stopRequested.store(true, std::memory_order_relaxed);
    if (searchThread_.joinable())
        searchThread_.join();
}

SearchResult Engine::runSearch_(const Position& root, const SearchLimits& limits) {
    lastDistributedReports_.clear();

    if (shouldUseDistributedSearch(limits, sharedSearchState_, distributedWorkers_)) {
        return runDistributedRootSplitSearch(
            root,
            limits,
            positionHistory_,
            static_cast<size_t>(option_("Hash").getValue<int>()),
            &sharedSearchState_,
            distributedWorkers_,
            &lastDistributedReports_
        );
    }

    const int threadCount = std::max(1, limits.threads);
    std::vector<SearchResult> workerResults(static_cast<size_t>(threadCount));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(threadCount));

    for (int workerId = 0; workerId < threadCount; ++workerId) {
        workers.emplace_back([&, workerId]() {
            Position workerRoot = root;
            Search worker(&tt_, &sharedSearchState_, workerId, positionHistory_);
            workerResults[static_cast<size_t>(workerId)] = worker.search(workerRoot, limits);
        });
    }

    for (std::thread& worker : workers) {
        if (worker.joinable())
            worker.join();
    }

    SearchResult aggregate{};
    if (!workerResults.empty())
        aggregate = workerResults[0];

    for (size_t workerId = 1; workerId < workerResults.size(); ++workerId)
        mergeSearchResult_(aggregate, workerResults[workerId], false);

    aggregate.telemetry.nodes = 0;
    aggregate.telemetry.qNodes = 0;
    aggregate.stopped = false;
    for (const SearchResult& workerResult : workerResults) {
        aggregate.telemetry.nodes += workerResult.telemetry.nodes;
        aggregate.telemetry.qNodes += workerResult.telemetry.qNodes;
        aggregate.stopped = aggregate.stopped || workerResult.stopped;
    }

    return aggregate;
}

void Engine::mergeSearchResult_(SearchResult& aggregate, const SearchResult& workerResult, bool preferWorker) {
    const bool workerHasDeeperResult = workerResult.telemetry.completedDepth > aggregate.telemetry.completedDepth;
    const bool sameDepth = workerResult.telemetry.completedDepth == aggregate.telemetry.completedDepth;
    const bool workerHasPreferredScore = sameDepth && (preferWorker || workerResult.score > aggregate.score);

    if (workerHasDeeperResult || workerHasPreferredScore)
        aggregate = workerResult;
}

void Engine::printSearchResult_(const SearchLimits& limits, const SearchResult& result, uint64_t elapsedMs) {
    const uint64_t totalNodes = result.telemetry.nodes + result.telemetry.qNodes;
    const auto nps = elapsedMs == 0 ? totalNodes : (1000 * totalNodes) / elapsedMs;
    const Depth reportedDepth =
        result.telemetry.completedDepth > 0 ? result.telemetry.completedDepth : std::min<Depth>(limits.depth, 1);
    std::cout << "info depth " << reportedDepth << ' ';
    printUciScore(result.score);
    std::cout << " nodes " << totalNodes << " time " << elapsedMs << " nps " << nps;
    printUciPV(result);
    std::cout << '\n';
    std::cout << "info string telemetry"
              << " nodes=" << result.telemetry.nodes
              << " qnodes=" << result.telemetry.qNodes
              << " elapsed_ms=" << result.telemetry.elapsedMs
              << " tt_hits=" << result.telemetry.ttHits
              << " tt_misses=" << result.telemetry.ttMisses
              << " tt_writes=" << result.telemetry.ttWrites
              << " tt_rewrites=" << result.telemetry.ttRewrites
              << " completed_depth=" << result.telemetry.completedDepth << '\n';

    for (const DistributedWorkerReport& report : lastDistributedReports_) {
        std::cout << "info string dist_worker"
                  << " endpoint=" << report.endpoint.host << ':' << report.endpoint.port
                  << " mode=" << (report.usedRemote ? "remote" : "local")
                  << " fallback_local=" << (report.fallbackToLocal ? 1 : 0)
                  << " assigned=" << report.assignedRootMoves.size()
                  << " moves=" << summarizeRootMoves(report.assignedRootMoves)
                  << " score=" << report.result.score
                  << " depth=" << report.result.telemetry.completedDepth
                  << " nodes=" << report.result.telemetry.nodes
                  << " qnodes=" << report.result.telemetry.qNodes
                  << " tt_hits=" << report.result.telemetry.ttHits
                  << " tt_misses=" << report.result.telemetry.ttMisses
                  << " tt_writes=" << report.result.telemetry.ttWrites
                  << " tt_rewrites=" << report.result.telemetry.ttRewrites
                  << " stopped=" << (report.result.stopped ? 1 : 0)
                  << " bestmove=" << to_string(report.result.bestMove) << '\n';
    }

    std::cout << "bestmove " << to_string(result.bestMove) << '\n';
    std::cout.flush();
}

}  // namespace engine
