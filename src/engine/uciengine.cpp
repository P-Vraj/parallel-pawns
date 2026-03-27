#include "uciengine.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <sstream>

#include "ucioption.h"

namespace engine {

namespace {

struct ParsedSetOption {
    std::string name;
    std::string value;
};

struct ParsedGo {
    std::optional<Depth> depth;
    bool infinite{false};
    std::optional<std::chrono::milliseconds> moveTime;
    std::optional<SearchLimits::TimeControl> timeControl;
};

std::optional<ParsedSetOption> parseSetOption(std::istringstream& iss) {
    std::string token;
    if (!(iss >> token) || token != "name")
        return std::nullopt;

    ParsedSetOption parsed;
    while (iss >> token) {
        if (token == "value")
            break;
        if (!parsed.name.empty())
            parsed.name += ' ';
        parsed.name += token;
    }

    if (parsed.name.empty())
        return std::nullopt;

    if (token == "value") {
        std::getline(iss >> std::ws, parsed.value);
        trim(parsed.value);
    }

    trim(parsed.name);
    return parsed;
}

ParsedGo parseGo(std::istringstream& iss) {
    ParsedGo parsed;
    SearchLimits::TimeControl timeControl;
    bool hasTimeControl = false;

    std::string token;
    while (iss >> token) {
        if (token == "depth") {
            Depth depth = 0;
            if (iss >> depth)
                parsed.depth = depth;
        }
        else if (token == "movetime") {
            int moveTimeMs = 0;
            if (iss >> moveTimeMs)
                parsed.moveTime = std::chrono::milliseconds(moveTimeMs);
        }
        else if (token == "wtime") {
            int timeMs = 0;
            if (iss >> timeMs) {
                timeControl.whiteTime = std::chrono::milliseconds(timeMs);
                hasTimeControl = true;
            }
        }
        else if (token == "btime") {
            int timeMs = 0;
            if (iss >> timeMs) {
                timeControl.blackTime = std::chrono::milliseconds(timeMs);
                hasTimeControl = true;
            }
        }
        else if (token == "winc") {
            int timeMs = 0;
            if (iss >> timeMs) {
                timeControl.whiteIncrement = std::chrono::milliseconds(timeMs);
                hasTimeControl = true;
            }
        }
        else if (token == "binc") {
            int timeMs = 0;
            if (iss >> timeMs) {
                timeControl.blackIncrement = std::chrono::milliseconds(timeMs);
                hasTimeControl = true;
            }
        }
        else if (token == "movestogo") {
            int movesToGo = 0;
            if (iss >> movesToGo) {
                timeControl.movesToGo = movesToGo;
                hasTimeControl = true;
            }
        }
        else if (token == "infinite") {
            parsed.infinite = true;
        }
    }

    if (hasTimeControl)
        parsed.timeControl = timeControl;

    return parsed;
}

}  // namespace

void UCIEngine::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        trim(command);
        if (command.empty())
            continue;

        if (command == "uci") {
            std::cout << "id name Parfait\n";
            std::cout << "id author Parallel Pawns\n\n";

            for (const auto& option : engine_.options_) {
                std::cout << option.uciDeclaration() << '\n';
            }

            std::cout << "\nuciok\n";
            std::cout.flush();
        }
        else if (command == "isready") {
            std::cout << "readyok\n";
            std::cout.flush();
        }
        else if (command == "setoption") {
            const auto parsed = parseSetOption(iss);
            if (!parsed.has_value()) {
                std::cout << "info string Invalid setoption command: " << line << '\n';
                std::cout.flush();
                continue;
            }
            engine_.setOption_(parsed->name, parsed->value);
        }
        else if (command == "ucinewgame") {
            engine_.setPosition_(startpos);
            engine_.tt_.clear();
        }
        else if (command == "position") {
            std::string fen{startpos};
            const size_t fenIt = line.find("fen");
            const size_t movesIt = line.find("moves");
            if (fenIt != std::string::npos) {
                fen = line.substr(fenIt + 4, movesIt - fenIt - 4);
                trim(fen);
            }
            engine_.setPosition_(fen);

            if (movesIt == std::string::npos)
                continue;
            iss.seekg(static_cast<std::streamoff>(movesIt + 6));

            for (std::string uciMove; iss >> uciMove;) {
                MoveList moveList(engine_.position_);
                auto* it = std::ranges::find_if(moveList, [&](Move m) { return to_string(m) == uciMove; });
                if (it != moveList.end()) {
                    const bool irreversible = Engine::isIrreversibleMove_(engine_.position_, *it);
                    UndoInfo u{};
                    engine_.position_.makeMove(*it, u);
                    engine_.recordCurrentPosition_(irreversible);
                }
                else {
                    std::cout << "info string Invalid move in position command: " << uciMove << '\n';
                    std::cout.flush();
                    continue;
                }
            }
        }
        else if (command == "go") {
            const ParsedGo go = parseGo(iss);
            engine_.startSearch_(go.depth, go.infinite, go.moveTime, go.timeControl);
        }
        else if (command == "stop") {
            engine_.stopSearch_();
        }
        else if (command == "quit") {
            engine_.stopSearch_();
            break;
        }
        else {
            std::cout << "info string Unknown command: " << line << '\n';
            std::cout.flush();
        }
    }
}

}  // namespace engine
