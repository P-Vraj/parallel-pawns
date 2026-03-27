#include "uciengine.h"

int main() {
    try {
        engine::UCIEngine uci;
        uci.loop();
    }
    catch (const std::exception& e) {
        std::cerr << "Error in main: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return 0;
}
