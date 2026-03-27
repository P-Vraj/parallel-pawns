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
        }
        else if (command == "isready") {
            std::cout << "readyok\n";
        }
        else if (command == "setoption") {
            const auto parsed = parseSetOption(iss);
            if (!parsed.has_value()) {
                std::cout << "Invalid setoption command: " << line << '\n';
                continue;
            }
            engine_.setOption_(parsed->name, parsed->value);
        }
        else if (command == "ucinewgame") {
            engine_.setPosition_(startpos);
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
                    UndoInfo u{};
                    engine_.position_.makeMove(*it, u);
                }
                else {
                    std::cout << "Invalid move in position command: " << uciMove << '\n';
                    continue;
                }
            }
        }
        else if (command == "go") {
            std::string token;
            Depth depth = 0;
            while (iss >> token) {
                if (token == "depth") {
                    iss >> depth;
                }
            }
            // Override engine's default depth for this search, before resetting it back to the default
            if (depth)
                engine_.searchLimits_.depth = depth;
            engine_.debugSearch_();
            engine_.searchLimits_.depth = static_cast<uint8_t>(engine_.option_("default depth").getValue<int>());
        }
        else if (command == "quit") {
            break;
        }
        else {
            std::cout << "Unknown command: " << line << '\n';
        }
    }
}

}  // namespace engine
