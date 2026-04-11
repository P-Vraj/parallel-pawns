#include "distributed_search.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstring>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <iostream>
#include <memory>

#include "engine.h"
#include "move_gen/generator.h"
#include "transposition_table.h"
#include "util.h"

namespace engine {

namespace {

struct SocketFd {
    int value{-1};

    SocketFd() = default;
    explicit SocketFd(int fd) noexcept : value(fd) {}
    SocketFd(const SocketFd&) = delete;
    SocketFd& operator=(const SocketFd&) = delete;

    SocketFd(SocketFd&& other) noexcept : value(std::exchange(other.value, -1)) {}
    SocketFd& operator=(SocketFd&& other) noexcept {
        if (this != &other) {
            reset();
            value = std::exchange(other.value, -1);
        }
        return *this;
    }

    ~SocketFd() { reset(); }

    [[nodiscard]] bool valid() const noexcept { return value >= 0; }

    void reset() noexcept {
        if (value >= 0) {
            ::close(value);
            value = -1;
        }
    }
};

struct WorkerRequest {
    std::string fen;
    std::vector<Key> history;
    std::vector<Move> rootMoves;
    SearchLimits limits;
    size_t hashMb{};
    std::optional<std::chrono::milliseconds> softLimit;
    std::optional<std::chrono::milliseconds> hardLimit;
};

struct WorkerResponse {
    SearchResult result;
    TTStats ttStats{};
};

enum class RequestReadStatus {
    Ok,
    Closed,
    Error
};

struct RemoteWorkerSession {
    DistributedWorkerEndpoint endpoint;
    SocketFd socket;
    bool available{false};
    bool failed{false};
    bool everConnected{false};
    int requestCount{0};
    int reconnectCount{0};
};

struct ConnectionLine {
    enum class Status {
        Line,
        Timeout,
        Closed,
        Error
    };

    Status status{Status::Error};
    std::string text;
};

ConnectionLine makeConnectionStatus(ConnectionLine::Status status) {
    return ConnectionLine{.status = status, .text = {}};
}

std::vector<std::string> split(std::string_view text, char delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find(delimiter, start);
        const size_t count = end == std::string_view::npos ? (text.size() - start) : (end - start);
        std::string part{text.substr(start, count)};
        trim(part);
        parts.push_back(std::move(part));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return parts;
}

template <typename Integer>
bool parseInteger(std::string_view text, Integer& value) {
    const auto* first = text.data();
    const auto* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    return ec == std::errc{} && ptr == last;
}

bool parseHexKey(std::string_view text, Key& value) {
    if (text.empty())
        return false;

    value = 0;
    for (const char ch : text) {
        value <<= 4;
        if (ch >= '0' && ch <= '9')
            value |= static_cast<Key>(ch - '0');
        else if (ch >= 'a' && ch <= 'f')
            value |= static_cast<Key>(10 + ch - 'a');
        else if (ch >= 'A' && ch <= 'F')
            value |= static_cast<Key>(10 + ch - 'A');
        else
            return false;
    }
    return true;
}

std::string serializeHistory(const std::vector<Key>& history) {
    std::ostringstream oss;
    oss << std::hex;
    for (size_t i = 0; i < history.size(); ++i) {
        if (i != 0)
            oss << ',';
        oss << history[i];
    }
    return oss.str();
}

std::string serializeMoves(const std::vector<Move>& moves) {
    std::string out;
    for (const Move move : moves) {
        if (!out.empty())
            out += ' ';
        out += to_string(move);
    }
    return out;
}

std::string serializePv(const SearchResult& result) {
    std::string out;
    for (uint8_t i = 0; i < result.pvLength; ++i) {
        if (!out.empty())
            out += ' ';
        out += to_string(result.pv[i]);
    }
    return out;
}

std::string summarizeRootMoves(std::span<const Move> moves, size_t maxMoves = 8) {
    std::string out;
    const size_t count = std::min(maxMoves, moves.size());
    for (size_t i = 0; i < count; ++i) {
        if (!out.empty())
            out += ',';
        out += to_string(moves[i]);
    }
    if (moves.size() > count)
        out += ",...";
    return out;
}

void logWorkerEvent(std::string_view event, std::string_view payload) {
    std::clog << "[dist_worker] event=" << event;
    if (!payload.empty())
        std::clog << ' ' << payload;
    std::clog << '\n';
    std::clog.flush();
}

std::optional<Move> findMoveByUci(const Position& pos, std::string_view uciMove) {
    MoveList legalMoves(pos);
    const auto it = std::ranges::find_if(legalMoves, [&](Move move) { return to_string(move) == uciMove; });
    if (it == legalMoves.end())
        return std::nullopt;
    return *it;
}

bool writeAll(int fd, std::string_view text) {
    size_t totalWritten = 0;
    while (totalWritten < text.size()) {
        const ssize_t written = ::send(fd, text.data() + totalWritten, text.size() - totalWritten, MSG_NOSIGNAL);
        if (written <= 0)
            return false;
        totalWritten += static_cast<size_t>(written);
    }
    return true;
}

ConnectionLine readLine(int fd, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
    if (timeout.has_value()) {
        pollfd descriptor{
            .fd = fd,
            .events = POLLIN,
            .revents = 0,
        };
        const int pollResult = ::poll(&descriptor, 1, static_cast<int>(timeout->count()));
        if (pollResult == 0)
            return makeConnectionStatus(ConnectionLine::Status::Timeout);
        if (pollResult < 0)
            return makeConnectionStatus(ConnectionLine::Status::Error);
        if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0)
            return makeConnectionStatus(ConnectionLine::Status::Error);
        if ((descriptor.revents & POLLHUP) != 0 && (descriptor.revents & POLLIN) == 0)
            return makeConnectionStatus(ConnectionLine::Status::Closed);
    }

    std::string line;
    char ch = '\0';
    while (true) {
        const ssize_t received = ::recv(fd, &ch, 1, 0);
        if (received == 0)
            return line.empty() ? makeConnectionStatus(ConnectionLine::Status::Closed)
                                : ConnectionLine{.status = ConnectionLine::Status::Line, .text = std::move(line)};
        if (received < 0)
            return makeConnectionStatus(ConnectionLine::Status::Error);
        if (ch == '\n')
            return ConnectionLine{.status = ConnectionLine::Status::Line, .text = std::move(line)};
        if (ch != '\r')
            line += ch;
    }
}

SocketFd connectToWorker(const DistributedWorkerEndpoint& endpoint) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;
    const std::string port = std::to_string(endpoint.port);
    if (::getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &results) != 0)
        return {};

    SocketFd socketFd;
    for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
        SocketFd candidate(::socket(current->ai_family, current->ai_socktype, current->ai_protocol));
        if (!candidate.valid())
            continue;
        if (::connect(candidate.value, current->ai_addr, current->ai_addrlen) == 0) {
            socketFd = std::move(candidate);
            break;
        }
    }

    ::freeaddrinfo(results);
    return socketFd;
}

bool endpointsEqual(
    const std::vector<DistributedWorkerEndpoint>& lhs,
    const std::vector<DistributedWorkerEndpoint>& rhs
) noexcept {
    if (lhs.size() != rhs.size())
        return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].host != rhs[i].host || lhs[i].port != rhs[i].port || lhs[i].threads != rhs[i].threads)
            return false;
    }
    return true;
}

SocketFd createServerSocket(std::string_view bindHost, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* results = nullptr;
    const std::string host = bindHost.empty() ? std::string{} : std::string(bindHost);
    const std::string portText = std::to_string(port);
    if (::getaddrinfo(host.empty() ? nullptr : host.c_str(), portText.c_str(), &hints, &results) != 0)
        return {};

    SocketFd serverSocket;
    int lastErrno = 0;
    for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
        SocketFd candidate(::socket(current->ai_family, current->ai_socktype, current->ai_protocol));
        if (!candidate.valid())
            continue;

        int reuse = 1;
        ::setsockopt(candidate.value, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        if (::bind(candidate.value, current->ai_addr, current->ai_addrlen) != 0) {
            lastErrno = errno;
            continue;
        }
        if (::listen(candidate.value, 16) != 0) {
            lastErrno = errno;
            continue;
        }

        serverSocket = std::move(candidate);
        break;
    }

    ::freeaddrinfo(results);
    if (!serverSocket.valid() && lastErrno != 0) {
        throw std::runtime_error(
            "failed to bind/listen on " + (host.empty() ? std::string("0.0.0.0") : host) + ':' + std::to_string(port) +
            ": " + std::strerror(lastErrno)
        );
    }
    return serverSocket;
}

SearchResult searchLocalSubset(
    TranspositionTable& tt,
    const Position& root,
    const SearchLimits& limits,
    const std::vector<Key>& positionHistory,
    SearchSharedState* sharedState,
    const std::vector<Move>& rootMoves,
    TTStats* ttStats = nullptr
) {
    tt.resetCounters();
    tt.newSearch();
    SearchResult result = runParallelSearch(
        root,
        limits,
        positionHistory,
        &tt,
        sharedState,
        std::span<const Move>(rootMoves.begin(), rootMoves.size())
    );

    if (ttStats != nullptr)
        *ttStats = tt.stats();

    return result;
}

SearchResult searchLocalSubset(
    const Position& root,
    const SearchLimits& limits,
    const std::vector<Key>& positionHistory,
    size_t hashMb,
    SearchSharedState* sharedState,
    const std::vector<Move>& rootMoves,
    TTStats* ttStats = nullptr
) {
    TranspositionTable tt(hashMb);
    return searchLocalSubset(tt, root, limits, positionHistory, sharedState, rootMoves, ttStats);
}

std::string buildRequestMessage(const WorkerRequest& request) {
    std::ostringstream oss;
    oss << "parfait_dist 1\n";
    oss << "fen " << request.fen << '\n';
    oss << "history " << serializeHistory(request.history) << '\n';
    oss << "rootmoves " << serializeMoves(request.rootMoves) << '\n';
    oss << "depth " << request.limits.depth << '\n';
    oss << "threads " << request.limits.threads << '\n';
    oss << "infinite " << (request.limits.infinite ? 1 : 0) << '\n';
    oss << "iterative " << (request.limits.iterativeDeepening ? 1 : 0) << '\n';
    oss << "movetime_ms " << request.limits.moveTime.value_or(std::chrono::milliseconds{-1}).count() << '\n';
    oss << "soft_ms " << request.softLimit.value_or(std::chrono::milliseconds{-1}).count() << '\n';
    oss << "hard_ms " << request.hardLimit.value_or(std::chrono::milliseconds{-1}).count() << '\n';
    oss << "hash_mb " << request.hashMb << '\n';
    oss << "end\n";
    return oss.str();
}

RequestReadStatus parseWorkerRequest(int fd, WorkerRequest& request, std::string& error) {
    std::string versionLine;
    const ConnectionLine firstLine = readLine(fd);
    if (firstLine.status == ConnectionLine::Status::Line)
        versionLine = firstLine.text;
    else {
        error = "request was empty";
        return firstLine.status == ConnectionLine::Status::Closed ? RequestReadStatus::Closed : RequestReadStatus::Error;
    }

    if (versionLine != "parfait_dist 1") {
        error = "unsupported protocol";
        return RequestReadStatus::Error;
    }

    std::string fen;
    std::vector<Key> history;
    std::vector<Move> rootMoves;
    SearchLimits limits;
    size_t hashMb = 256;
    std::optional<std::chrono::milliseconds> softLimit;
    std::optional<std::chrono::milliseconds> hardLimit;

    while (true) {
        const ConnectionLine line = readLine(fd);
        if (line.status != ConnectionLine::Status::Line) {
            error = "unexpected end of request";
            return line.status == ConnectionLine::Status::Closed ? RequestReadStatus::Closed : RequestReadStatus::Error;
        }
        if (line.text == "end")
            break;

        const size_t splitPos = line.text.find(' ');
        const std::string key = splitPos == std::string::npos ? line.text : line.text.substr(0, splitPos);
        const std::string value = splitPos == std::string::npos ? std::string{} : line.text.substr(splitPos + 1);

        if (key == "fen") {
            fen = value;
        }
        else if (key == "history") {
            if (!value.empty()) {
                for (const std::string& token : split(value, ',')) {
                    if (token.empty())
                        continue;
                    Key parsed = 0;
                    if (!parseHexKey(token, parsed)) {
                        error = "invalid history key";
                        return RequestReadStatus::Error;
                    }
                    history.push_back(parsed);
                }
            }
        }
        else if (key == "rootmoves") {
            Position position = Position::fromFEN(fen);
            std::istringstream iss(value);
            for (std::string moveText; iss >> moveText;) {
                const auto move = findMoveByUci(position, moveText);
                if (!move.has_value()) {
                    error = "invalid root move: " + moveText;
                    return RequestReadStatus::Error;
                }
                rootMoves.push_back(*move);
            }
        }
        else if (key == "depth") {
            if (!parseInteger(value, limits.depth)) {
                error = "invalid depth";
                return RequestReadStatus::Error;
            }
        }
        else if (key == "threads") {
            if (!parseInteger(value, limits.threads)) {
                error = "invalid threads";
                return RequestReadStatus::Error;
            }
        }
        else if (key == "infinite") {
            int raw = 0;
            if (!parseInteger(value, raw)) {
                error = "invalid infinite flag";
                return RequestReadStatus::Error;
            }
            limits.infinite = raw != 0;
        }
        else if (key == "iterative") {
            int raw = 0;
            if (!parseInteger(value, raw)) {
                error = "invalid iterative flag";
                return RequestReadStatus::Error;
            }
            limits.iterativeDeepening = raw != 0;
        }
        else if (key == "movetime_ms") {
            int64_t raw = -1;
            if (!parseInteger(value, raw)) {
                error = "invalid movetime";
                return RequestReadStatus::Error;
            }
            if (raw >= 0)
                limits.moveTime = std::chrono::milliseconds(raw);
        }
        else if (key == "soft_ms" || key == "hard_ms") {
            int64_t raw = -1;
            if (!parseInteger(value, raw)) {
                error = "invalid deadline";
                return RequestReadStatus::Error;
            }
            if (raw >= 0) {
                if (key == "soft_ms")
                    softLimit = std::chrono::milliseconds(raw);
                else
                    hardLimit = std::chrono::milliseconds(raw);
            }
        }
        else if (key == "hash_mb") {
            if (!parseInteger(value, hashMb)) {
                error = "invalid hash size";
                return RequestReadStatus::Error;
            }
        }
    }

    if (fen.empty()) {
        error = "missing fen";
        return RequestReadStatus::Error;
    }
    if (rootMoves.empty()) {
        error = "missing root moves";
        return RequestReadStatus::Error;
    }
    if (history.empty())
        history.push_back(Position::fromFEN(fen).hash());

    request.fen = std::move(fen);
    request.history = std::move(history);
    request.rootMoves = std::move(rootMoves);
    request.limits = limits;
    request.hashMb = hashMb;
    request.softLimit = softLimit;
    request.hardLimit = hardLimit;
    return RequestReadStatus::Ok;
}

std::string buildResponseMessage(const WorkerResponse& response) {
    std::ostringstream oss;
    oss << "ok 1\n";
    oss << "score " << response.result.score << '\n';
    oss << "bestmove " << to_string(response.result.bestMove) << '\n';
    oss << "pv " << serializePv(response.result) << '\n';
    oss << "completed_depth " << response.result.telemetry.completedDepth << '\n';
    oss << "nodes " << response.result.telemetry.nodes << '\n';
    oss << "qnodes " << response.result.telemetry.qNodes << '\n';
    oss << "stopped " << (response.result.stopped ? 1 : 0) << '\n';
    oss << "tt_hits " << response.ttStats.hits << '\n';
    oss << "tt_misses " << response.ttStats.misses << '\n';
    oss << "tt_writes " << response.ttStats.writes << '\n';
    oss << "tt_rewrites " << response.ttStats.rewrites << '\n';
    oss << "end\n";
    return oss.str();
}

bool parseResponse(
    int fd,
    const Position& root,
    const SearchSharedState* sharedState,
    std::atomic<bool>& stopSent,
    WorkerResponse& response
) {
    std::string line;
    while (true) {
        const ConnectionLine firstLine = readLine(fd, std::chrono::milliseconds(50));
        if (firstLine.status == ConnectionLine::Status::Timeout) {
            if (sharedState != nullptr && sharedState->stopRequested.load(std::memory_order_relaxed) &&
                !stopSent.exchange(true, std::memory_order_relaxed)) {
                writeAll(fd, "stop\n");
            }
            continue;
        }
        if (firstLine.status != ConnectionLine::Status::Line)
            return false;
        line = firstLine.text;
        break;
    }

    if (line != "ok 1")
        return false;

    SearchResult result{};
    TTStats ttStats{};
    while (true) {
        const ConnectionLine responseLine = readLine(fd, std::chrono::milliseconds(50));
        if (responseLine.status == ConnectionLine::Status::Timeout) {
            if (sharedState != nullptr && sharedState->stopRequested.load(std::memory_order_relaxed) &&
                !stopSent.exchange(true, std::memory_order_relaxed)) {
                writeAll(fd, "stop\n");
            }
            continue;
        }
        if (responseLine.status != ConnectionLine::Status::Line)
            return false;
        if (responseLine.text == "end")
            break;

        const size_t splitPos = responseLine.text.find(' ');
        const std::string key =
            splitPos == std::string::npos ? responseLine.text : responseLine.text.substr(0, splitPos);
        const std::string value =
            splitPos == std::string::npos ? std::string{} : responseLine.text.substr(splitPos + 1);

        if (key == "score") {
            if (!parseInteger(value, result.score))
                return false;
        }
        else if (key == "bestmove") {
            if (value == "0000") {
                result.bestMove = Move::none();
                continue;
            }
            const auto bestMove = findMoveByUci(root, value);
            if (!bestMove.has_value())
                return false;
            result.bestMove = *bestMove;
        }
        else if (key == "pv") {
            Position pvPosition = root;
            std::istringstream iss(value);
            std::string moveText;
            while (iss >> moveText && result.pvLength < kMaxPly) {
                const auto move = findMoveByUci(pvPosition, moveText);
                if (!move.has_value())
                    return false;
                result.pv[result.pvLength++] = *move;
                UndoInfo undo{};
                pvPosition.makeMove(*move, undo);
            }
        }
        else if (key == "completed_depth") {
            if (!parseInteger(value, result.telemetry.completedDepth))
                return false;
        }
        else if (key == "nodes") {
            if (!parseInteger(value, result.telemetry.nodes))
                return false;
        }
        else if (key == "qnodes") {
            if (!parseInteger(value, result.telemetry.qNodes))
                return false;
        }
        else if (key == "stopped") {
            int raw = 0;
            if (!parseInteger(value, raw))
                return false;
            result.stopped = raw != 0;
        }
        else if (key == "tt_hits") {
            if (!parseInteger(value, ttStats.hits))
                return false;
        }
        else if (key == "tt_misses") {
            if (!parseInteger(value, ttStats.misses))
                return false;
        }
        else if (key == "tt_writes") {
            if (!parseInteger(value, ttStats.writes))
                return false;
        }
        else if (key == "tt_rewrites") {
            if (!parseInteger(value, ttStats.rewrites))
                return false;
        }
    }

    response.result = result;
    response.ttStats = ttStats;
    return true;
}

void handleWorkerClient(int clientFd) {
    std::unique_ptr<TranspositionTable> sessionTt;
    size_t sessionHashMb = 0;
    int sessionRequestCount = 0;

    while (true) {
        WorkerRequest request;
        std::string error;
        const RequestReadStatus status = parseWorkerRequest(clientFd, request, error);
        if (status == RequestReadStatus::Closed)
            return;
        if (status != RequestReadStatus::Ok) {
            logWorkerEvent("request_error", error);
            const std::string message = "error " + error + "\nend\n";
            writeAll(clientFd, message);
            return;
        }

        if (!sessionTt || sessionHashMb != request.hashMb) {
            sessionTt = std::make_unique<TranspositionTable>(request.hashMb);
            sessionHashMb = request.hashMb;
            sessionRequestCount = 0;
        }

        Position root = Position::fromFEN(request.fen);
        const bool sessionReused = sessionRequestCount > 0;
        logWorkerEvent(
            "request_start",
            "root_moves=" + std::to_string(request.rootMoves.size()) +
                " sample_moves=" + summarizeRootMoves(request.rootMoves) +
                " depth=" + std::to_string(request.limits.depth) +
                " threads=" + std::to_string(request.limits.threads) +
                " session_reused=" + std::to_string(sessionReused ? 1 : 0) +
                " session_request=" + std::to_string(sessionRequestCount + 1) +
                " movetime_ms=" + std::to_string(request.limits.moveTime.value_or(std::chrono::milliseconds{-1}).count()) +
                " hash_mb=" + std::to_string(request.hashMb)
        );
        SearchSharedState sharedState{};
        const auto now = std::chrono::steady_clock::now();
        if (request.softLimit.has_value())
            sharedState.softDeadline = now + *request.softLimit;
        if (request.hardLimit.has_value())
            sharedState.hardDeadline = now + *request.hardLimit;
        TTStats ttStats{};
        std::atomic<bool> searchFinished{false};
        SearchResult result{};
        std::thread searchThread([&]() {
            result = searchLocalSubset(
                *sessionTt,
                root,
                request.limits,
                request.history,
                &sharedState,
                request.rootMoves,
                &ttStats
            );
            searchFinished.store(true, std::memory_order_relaxed);
        });

        while (!searchFinished.load(std::memory_order_relaxed)) {
            const ConnectionLine control = readLine(clientFd, std::chrono::milliseconds(50));
            if (control.status == ConnectionLine::Status::Timeout)
                continue;
            if (control.status == ConnectionLine::Status::Closed || control.status == ConnectionLine::Status::Error) {
                sharedState.stopRequested.store(true, std::memory_order_relaxed);
                logWorkerEvent("request_stop", "reason=disconnect");
                break;
            }
            if (control.status == ConnectionLine::Status::Line && control.text == "stop") {
                sharedState.stopRequested.store(true, std::memory_order_relaxed);
                logWorkerEvent("request_stop", "reason=coordinator");
                break;
            }
        }

        if (searchThread.joinable())
            searchThread.join();

        result.telemetry.ttHits = ttStats.hits;
        result.telemetry.ttMisses = ttStats.misses;
        result.telemetry.ttWrites = ttStats.writes;
        result.telemetry.ttRewrites = ttStats.rewrites;
        ++sessionRequestCount;
        logWorkerEvent(
            "request_done",
            "root_moves=" + std::to_string(request.rootMoves.size()) +
                " nodes=" + std::to_string(result.telemetry.nodes) +
                " qnodes=" + std::to_string(result.telemetry.qNodes) +
                " tt_hits=" + std::to_string(result.telemetry.ttHits) +
                " tt_misses=" + std::to_string(result.telemetry.ttMisses) +
                " tt_writes=" + std::to_string(result.telemetry.ttWrites) +
                " tt_rewrites=" + std::to_string(result.telemetry.ttRewrites) +
                " depth=" + std::to_string(result.telemetry.completedDepth) +
                " session_reused=" + std::to_string(sessionReused ? 1 : 0) +
                " session_request=" + std::to_string(sessionRequestCount) +
                " stopped=" + std::to_string(result.stopped ? 1 : 0) +
                " bestmove=" + to_string(result.bestMove)
        );

        if (!writeAll(clientFd, buildResponseMessage(WorkerResponse{.result = result, .ttStats = ttStats})))
            return;
    }
}

void mergeResult(SearchResult& aggregate, const SearchResult& candidate, bool& hasAggregate) {
    if (!hasAggregate) {
        aggregate = candidate;
        hasAggregate = true;
        return;
    }

    const bool deeper = candidate.telemetry.completedDepth > aggregate.telemetry.completedDepth;
    const bool sameDepth = candidate.telemetry.completedDepth == aggregate.telemetry.completedDepth;
    if (deeper || (sameDepth && candidate.score > aggregate.score))
        aggregate = candidate;
}

}  // namespace

struct DistributedCoordinatorSessions::Impl {
    std::vector<DistributedWorkerEndpoint> endpoints;
    std::vector<RemoteWorkerSession> remoteSessions;
    std::vector<std::unique_ptr<TranspositionTable>> localTables;
    std::vector<int> localRequestCounts;
};

DistributedCoordinatorSessions::DistributedCoordinatorSessions() : impl_(std::make_unique<Impl>()) {}

DistributedCoordinatorSessions::~DistributedCoordinatorSessions() = default;

DistributedCoordinatorSessions::DistributedCoordinatorSessions(DistributedCoordinatorSessions&&) noexcept = default;

DistributedCoordinatorSessions& DistributedCoordinatorSessions::operator=(DistributedCoordinatorSessions&&) noexcept = default;

void DistributedCoordinatorSessions::reset() {
    impl_->remoteSessions.clear();
    impl_->localTables.clear();
    impl_->localRequestCounts.clear();
    impl_->endpoints.clear();
}

void DistributedCoordinatorSessions::setEndpoints(const std::vector<DistributedWorkerEndpoint>& endpoints) {
    if (endpointsEqual(impl_->endpoints, endpoints))
        return;

    impl_->remoteSessions.clear();
    impl_->localTables.clear();
    impl_->localRequestCounts.clear();
    impl_->endpoints = endpoints;
}

bool parseDistributedWorkerEndpoints(
    std::string_view rawEndpoints,
    std::vector<DistributedWorkerEndpoint>& endpoints,
    std::string& error
) {
    endpoints.clear();
    error.clear();

    std::string raw{rawEndpoints};
    trim(raw);
    if (raw.empty())
        return true;

    for (const std::string& token : split(raw, ',')) {
        if (token.empty())
            continue;

        DistributedWorkerEndpoint endpoint;
        std::string address = token;
        const size_t at = address.rfind('@');
        if (at != std::string::npos) {
            int threads = 0;
            const std::string threadText = address.substr(at + 1);
            if (!parseInteger(threadText, threads) || threads <= 0) {
                error = "invalid worker thread count: " + token;
                return false;
            }
            endpoint.threads = threads;
            address = address.substr(0, at);
        }

        const size_t colon = address.rfind(':');
        if (colon == std::string::npos) {
            endpoint.host = "127.0.0.1";
            uint16_t port = 0;
            if (!parseInteger(address, port) || port == 0) {
                error = "invalid worker endpoint: " + token;
                return false;
            }
            endpoint.port = port;
        }
        else {
            endpoint.host = address.substr(0, colon);
            trim(endpoint.host);
            if (endpoint.host.empty())
                endpoint.host = "127.0.0.1";

            uint16_t port = 0;
            const std::string portText = address.substr(colon + 1);
            if (!parseInteger(portText, port) || port == 0) {
                error = "invalid worker endpoint: " + token;
                return false;
            }
            endpoint.port = port;
        }

        endpoints.push_back(std::move(endpoint));
    }

    return true;
}

bool parseDistributedWorkerConfigFile(
    std::string_view path,
    std::vector<DistributedWorkerEndpoint>& endpoints,
    std::string& error
) {
    endpoints.clear();
    error.clear();

    std::ifstream input{std::string(path)};
    if (!input.is_open()) {
        error = "could not open file";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string endpointText;
        if (!(iss >> endpointText))
            continue;

        DistributedWorkerEndpoint endpoint;
        std::string parseError;
        std::vector<DistributedWorkerEndpoint> parsed;
        if (!parseDistributedWorkerEndpoints(endpointText, parsed, parseError) || parsed.size() != 1) {
            error = "line " + std::to_string(lineNumber) + ": " + parseError;
            return false;
        }
        endpoint = parsed.front();

        int explicitThreads = 0;
        if (iss >> explicitThreads) {
            if (explicitThreads <= 0) {
                error = "line " + std::to_string(lineNumber) + ": invalid thread count";
                return false;
            }
            endpoint.threads = explicitThreads;
        }

        endpoints.push_back(std::move(endpoint));
    }

    return true;
}

SearchResult runDistributedRootSplitSearch(
    const Position& root,
    const SearchLimits& limits,
    const std::vector<Key>& positionHistory,
    size_t hashMb,
    SearchSharedState* sharedState,
    const std::vector<DistributedWorkerEndpoint>& endpoints,
    DistributedCoordinatorSessions* sessions,
    std::vector<DistributedWorkerReport>* reports
) {
    MoveList legalMoves(root);
    std::vector<Move> allRootMoves(legalMoves.begin(), legalMoves.end());
    if (allRootMoves.empty()) {
        TTStats ttStats{};
        return searchLocalSubset(root, limits, positionHistory, hashMb, sharedState, allRootMoves, &ttStats);
    }

    const size_t participantCount = std::min(endpoints.size() + 1, allRootMoves.size());
    const size_t remoteWorkerCount = participantCount > 0 ? std::min(endpoints.size(), participantCount - 1) : 0;

    std::vector<std::vector<Move>> assignments(participantCount);
    for (size_t i = 0; i < allRootMoves.size(); ++i)
        assignments[i % participantCount].push_back(allRootMoves[i]);

    std::vector<std::unique_ptr<TranspositionTable>> ephemeralLocalTables;
    std::vector<RemoteWorkerSession> ephemeralRemoteSessions;
    std::vector<int> ephemeralLocalRequestCounts;

    std::vector<std::unique_ptr<TranspositionTable>>* localTables = &ephemeralLocalTables;
    std::vector<RemoteWorkerSession>* remoteSessions = &ephemeralRemoteSessions;
    std::vector<int>* localRequestCounts = &ephemeralLocalRequestCounts;
    if (sessions != nullptr) {
        sessions->setEndpoints(endpoints);
        localTables = &sessions->impl_->localTables;
        remoteSessions = &sessions->impl_->remoteSessions;
        localRequestCounts = &sessions->impl_->localRequestCounts;
    }

    if (localTables->size() < participantCount)
        localTables->resize(participantCount);
    else if (localTables->size() > participantCount)
        localTables->resize(participantCount);

    if (localRequestCounts->size() < participantCount)
        localRequestCounts->resize(participantCount, 0);
    else if (localRequestCounts->size() > participantCount)
        localRequestCounts->resize(participantCount);

    if (remoteSessions->size() < remoteWorkerCount)
        remoteSessions->resize(remoteWorkerCount);
    else if (remoteSessions->size() > remoteWorkerCount)
        remoteSessions->resize(remoteWorkerCount);

    for (size_t i = 0; i < participantCount; ++i) {
        if ((*localTables)[i] == nullptr)
            (*localTables)[i] = std::make_unique<TranspositionTable>(hashMb);
    }

    for (size_t remoteIndex = 0; remoteIndex < remoteWorkerCount; ++remoteIndex) {
        RemoteWorkerSession& session = (*remoteSessions)[remoteIndex];
        session.endpoint = endpoints[remoteIndex];
        if (!session.socket.valid()) {
            if (session.everConnected)
                ++session.reconnectCount;
            session.socket = connectToWorker(endpoints[remoteIndex]);
            session.available = session.socket.valid();
            session.failed = !session.available;
            session.everConnected = session.everConnected || session.available;
        }
        else {
            session.available = true;
        }
    }

    struct IterationState {
        std::vector<SearchResult> results;
        std::vector<DistributedWorkerReport> reports;
    };

    const auto shouldAbortBeforeNextIteration = [&]() {
        if (sharedState == nullptr)
            return false;
        if (sharedState->stopRequested.load(std::memory_order_relaxed))
            return true;

        const auto now = std::chrono::steady_clock::now();
        return (sharedState->hardDeadline.has_value() && *sharedState->hardDeadline <= now) ||
               (sharedState->softDeadline.has_value() && *sharedState->softDeadline <= now);
    };

    const auto runIteration = [&](Depth depth) {
        IterationState state{
            .results = std::vector<SearchResult>(participantCount),
            .reports = std::vector<DistributedWorkerReport>(participantCount),
        };
        std::vector<TTStats> iterationTtStats(participantCount);
        std::vector<std::thread> workers;
        workers.reserve(participantCount);

        workers.emplace_back([&]() {
            constexpr size_t coordinatorIndex = 0;
            SearchLimits iterationLimits = limits;
            iterationLimits.depth = depth;
            iterationLimits.iterativeDeepening = false;

            state.reports[coordinatorIndex].endpoint = DistributedWorkerEndpoint{
                .host = "coordinator",
                .port = 0,
                .threads = limits.threads,
            };
            state.reports[coordinatorIndex].assignedRootMoves = assignments[coordinatorIndex];
            state.reports[coordinatorIndex].coordinatorParticipant = true;
            state.reports[coordinatorIndex].sessionReused = (*localRequestCounts)[coordinatorIndex] > 0;
            state.reports[coordinatorIndex].sessionRequestCount = (*localRequestCounts)[coordinatorIndex] + 1;

            state.results[coordinatorIndex] = searchLocalSubset(
                *(*localTables)[coordinatorIndex],
                root,
                iterationLimits,
                positionHistory,
                sharedState,
                assignments[coordinatorIndex],
                &iterationTtStats[coordinatorIndex]
            );
            state.results[coordinatorIndex].telemetry.ttHits = iterationTtStats[coordinatorIndex].hits;
            state.results[coordinatorIndex].telemetry.ttMisses = iterationTtStats[coordinatorIndex].misses;
            state.results[coordinatorIndex].telemetry.ttWrites = iterationTtStats[coordinatorIndex].writes;
            state.results[coordinatorIndex].telemetry.ttRewrites = iterationTtStats[coordinatorIndex].rewrites;
            ++(*localRequestCounts)[coordinatorIndex];
            state.reports[coordinatorIndex].result = state.results[coordinatorIndex];
        });

        for (size_t remoteIndex = 0; remoteIndex < remoteWorkerCount; ++remoteIndex) {
            workers.emplace_back([&, remoteIndex, depth]() {
                const size_t resultIndex = remoteIndex + 1;
                const auto now = std::chrono::steady_clock::now();
                const auto remainingSoft =
                    sharedState != nullptr && sharedState->softDeadline.has_value()
                        ? std::optional<std::chrono::milliseconds>(
                              std::chrono::milliseconds(std::max<int64_t>(
                                  0,
                                  std::chrono::duration_cast<std::chrono::milliseconds>(*sharedState->softDeadline - now)
                                      .count()
                              ))
                          )
                        : std::nullopt;
                const auto remainingHard =
                    sharedState != nullptr && sharedState->hardDeadline.has_value()
                        ? std::optional<std::chrono::milliseconds>(
                              std::chrono::milliseconds(std::max<int64_t>(
                                  0,
                                  std::chrono::duration_cast<std::chrono::milliseconds>(*sharedState->hardDeadline - now)
                                      .count()
                              ))
                          )
                        : std::nullopt;

                state.reports[resultIndex].endpoint = endpoints[remoteIndex];
                state.reports[resultIndex].assignedRootMoves = assignments[resultIndex];

                const WorkerRequest request{
                    .fen = root.toFEN(),
                    .history = positionHistory,
                    .rootMoves = assignments[resultIndex],
                    .limits = SearchLimits{
                        .depth = depth,
                        .threads = endpoints[remoteIndex].threads,
                        .infinite = limits.infinite,
                        .iterativeDeepening = false,
                        .moveTime = limits.moveTime,
                        .timeControl = limits.timeControl,
                    },
                    .hashMb = hashMb,
                    .softLimit = remainingSoft,
                    .hardLimit = remainingHard
                };

                RemoteWorkerSession& remoteSession = (*remoteSessions)[remoteIndex];
                state.reports[resultIndex].usedRemote = remoteSession.available;
                state.reports[resultIndex].sessionReused = remoteSession.requestCount > 0;
                state.reports[resultIndex].sessionRequestCount = remoteSession.requestCount + 1;
                state.reports[resultIndex].sessionReconnectCount = remoteSession.reconnectCount;

                if (remoteSession.available && !writeAll(remoteSession.socket.value, buildRequestMessage(request))) {
                    remoteSession.available = false;
                    remoteSession.failed = true;
                    remoteSession.socket.reset();
                }

                if (!remoteSession.available) {
                    state.results[resultIndex] = searchLocalSubset(
                        *(*localTables)[resultIndex],
                        root,
                        request.limits,
                        positionHistory,
                        sharedState,
                        assignments[resultIndex],
                        &iterationTtStats[resultIndex]
                    );
                    state.reports[resultIndex].sessionReused = (*localRequestCounts)[resultIndex] > 0;
                    state.reports[resultIndex].sessionRequestCount = (*localRequestCounts)[resultIndex] + 1;
                    state.results[resultIndex].telemetry.ttHits = iterationTtStats[resultIndex].hits;
                    state.results[resultIndex].telemetry.ttMisses = iterationTtStats[resultIndex].misses;
                    state.results[resultIndex].telemetry.ttWrites = iterationTtStats[resultIndex].writes;
                    state.results[resultIndex].telemetry.ttRewrites = iterationTtStats[resultIndex].rewrites;
                    ++(*localRequestCounts)[resultIndex];
                    state.reports[resultIndex].fallbackToLocal = true;
                    state.reports[resultIndex].result = state.results[resultIndex];
                    return;
                }

                WorkerResponse response;
                std::atomic<bool> stopSent{false};
                if (!parseResponse(remoteSession.socket.value, root, sharedState, stopSent, response)) {
                    remoteSession.available = false;
                    remoteSession.failed = true;
                    remoteSession.socket.reset();
                    state.results[resultIndex] = searchLocalSubset(
                        *(*localTables)[resultIndex],
                        root,
                        request.limits,
                        positionHistory,
                        sharedState,
                        assignments[resultIndex],
                        &iterationTtStats[resultIndex]
                    );
                    state.reports[resultIndex].sessionReused = (*localRequestCounts)[resultIndex] > 0;
                    state.reports[resultIndex].sessionRequestCount = (*localRequestCounts)[resultIndex] + 1;
                    state.results[resultIndex].telemetry.ttHits = iterationTtStats[resultIndex].hits;
                    state.results[resultIndex].telemetry.ttMisses = iterationTtStats[resultIndex].misses;
                    state.results[resultIndex].telemetry.ttWrites = iterationTtStats[resultIndex].writes;
                    state.results[resultIndex].telemetry.ttRewrites = iterationTtStats[resultIndex].rewrites;
                    ++(*localRequestCounts)[resultIndex];
                    state.reports[resultIndex].fallbackToLocal = true;
                    state.reports[resultIndex].result = state.results[resultIndex];
                    return;
                }

                state.results[resultIndex] = response.result;
                ++remoteSession.requestCount;
                state.reports[resultIndex].sessionRequestCount = remoteSession.requestCount;
                state.reports[resultIndex].sessionReconnectCount = remoteSession.reconnectCount;
                iterationTtStats[resultIndex] = response.ttStats;
                state.results[resultIndex].telemetry.ttHits = iterationTtStats[resultIndex].hits;
                state.results[resultIndex].telemetry.ttMisses = iterationTtStats[resultIndex].misses;
                state.results[resultIndex].telemetry.ttWrites = iterationTtStats[resultIndex].writes;
                state.results[resultIndex].telemetry.ttRewrites = iterationTtStats[resultIndex].rewrites;
                state.reports[resultIndex].result = state.results[resultIndex];
            });
        }

        for (std::thread& worker : workers) {
            if (worker.joinable())
                worker.join();
        }

        return state;
    };

    SearchResult aggregate{};
    aggregate.bestMove = allRootMoves.front();
    aggregate.pv[0] = aggregate.bestMove;
    aggregate.pvLength = 1;
    aggregate.stopped = false;

    std::vector<DistributedWorkerReport> latestReports;
    const Depth targetDepth = limits.infinite ? static_cast<Depth>(kMaxPly - 1) : limits.depth;

    for (Depth currentDepth = 1; currentDepth <= targetDepth; ++currentDepth) {
        if (shouldAbortBeforeNextIteration()) {
            aggregate.stopped = true;
            break;
        }

        IterationState iteration = runIteration(currentDepth);
        latestReports = std::move(iteration.reports);

        SearchResult iterationBest{};
        bool hasIterationBest = false;
        bool iterationComplete = true;
        bool iterationStopped = false;

        for (const SearchResult& result : iteration.results) {
            aggregate.telemetry.nodes += result.telemetry.nodes;
            aggregate.telemetry.qNodes += result.telemetry.qNodes;
            aggregate.telemetry.ttHits += result.telemetry.ttHits;
            aggregate.telemetry.ttMisses += result.telemetry.ttMisses;
            aggregate.telemetry.ttWrites += result.telemetry.ttWrites;
            aggregate.telemetry.ttRewrites += result.telemetry.ttRewrites;

            iterationStopped = iterationStopped || result.stopped;
            if (result.telemetry.completedDepth < currentDepth) {
                iterationComplete = false;
                continue;
            }

            mergeResult(iterationBest, result, hasIterationBest);
        }

        if (!iterationComplete || !hasIterationBest) {
            aggregate.stopped = true;
            break;
        }

        aggregate.score = iterationBest.score;
        aggregate.bestMove = iterationBest.bestMove;
        aggregate.pv = iterationBest.pv;
        aggregate.pvLength = iterationBest.pvLength;
        aggregate.telemetry.completedDepth = currentDepth;
        aggregate.stopped = iterationStopped;

        if (sharedState != nullptr && sharedState->stopRequested.load(std::memory_order_relaxed)) {
            aggregate.stopped = true;
            break;
        }
    }

    if (reports != nullptr)
        *reports = std::move(latestReports);

    return aggregate;
}

int runDistributedWorkerServer(std::string_view bindHost, uint16_t port) {
    init_engine();

    SocketFd serverSocket = createServerSocket(bindHost, port);
    if (!serverSocket.valid()) {
        throw std::runtime_error(
            "failed to bind distributed worker socket on " + std::string(bindHost) + ':' + std::to_string(port)
        );
    }

    while (true) {
        sockaddr_storage clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        const int clientFd =
            ::accept(serverSocket.value, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (clientFd < 0)
            continue;

        std::thread([client = SocketFd(clientFd)]() mutable {
            handleWorkerClient(client.value);
        }).detach();
    }

    return 0;
}

}  // namespace engine
