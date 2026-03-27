#include "uciengine.h"

int main() {
    try {
        engine::UCIEngine uci;
        uci.loop();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return 0;
}
