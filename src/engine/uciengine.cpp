#include "uciengine.h"

#include <sstream>

namespace engine {

void UCIEngine::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "uci") {
            std::cout << "id name Parfait\n";
            std::cout << "id author Parallel Pawns\n\n";

            for (const auto& [name, option] : engine_.options_) {
                std::cout << "option name " << name << '\n';
            }

            std::cout << "\nuciok\n";
        }
        else if (command == "isready") {
            std::cout << "readyok\n";
        }
        else if (command == "setoption") {
            std::string token, name, value;
            while (iss >> token) {
                if (token == "name") {
                    // Parse possibly multi-word option name
                    name.clear();
                    value.clear();
                    std::string word;
                    while (iss >> word && word != "value") {
                        if (!name.empty())
                            name += " ";
                        name += word;
                    }
                    // Parse possibly multi-word value, if found
                    if (word == "value") {
                        std::string valueWord;
                        while (iss >> valueWord) {
                            if (!value.empty())
                                value += " ";
                            value += valueWord;
                        }
                    }
                    trim(name);
                    trim(value);
                    if (name.empty()) {
                        std::cout << "Invalid setoption command: " << line << '\n';
                        continue;
                    }
                    if (value == "true" || value == "false" || value == "on" || value == "off")
                        engine_.setOption_(name, value == "true" || value == "on");
                    else if (!value.empty() && std::ranges::all_of(value, ::isdigit))
                        engine_.setOption_(name, std::stoi(value));
                    else if (!value.empty())
                        engine_.setOption_(name, value);
                    else
                        engine_.setOption_(name, true);  // If no value, treat it as a boolean (true)
                }
            }
        }
        else if (command == "ucinewgame") {
            engine_.setPosition_(startpos);
        }
        else if (command == "position") {
            std::string token, fen;
            iss >> token;
            if (token == "startpos") {
                fen = std::string(startpos);
            }
            else if (token == "fen") {
                std::string fenPart;
                for (int i = 0; i < 6 && iss >> fenPart; ++i) {
                    if (!fen.empty())
                        fen += " ";
                    fen += fenPart;
                }
                trim(fen);
                iss >> token;  // Check if there are moves after the FEN
            }
            else {
                std::cout << "Invalid position command: " << line << '\n';
                continue;
            }
            engine_.setPosition_(fen);
            if (token == "moves") {
                // Can be refactored
                std::string uciMove;
                while (iss >> uciMove) {
                    MoveList moves(engine_.position_);
                    auto* it = std::ranges::find_if(moves, [&](Move m) { return to_string(m) == uciMove; });
                    if (it != moves.end()) {
                        UndoInfo u{};
                        engine_.position_.makeMove(*it, u);
                    }
                    else {
                        std::cout << "Invalid move in position command: " << uciMove << '\n';
                        continue;
                    }
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
            engine_.searchLimits_.depth = std::get<int>(engine_.options_["default depth"]);
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