#include <cstdlib>
#include <iostream>
#include <string_view>

#include "distributed_search.h"
#include "uciengine.h"

int main(int argc, char* argv[]) {
    try {
        std::string_view workerBindHost = "127.0.0.1";
        uint16_t workerPort = 0;

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--worker-bind" && i + 1 < argc) {
                workerBindHost = argv[++i];
            }
            else if (arg == "--worker-port" && i + 1 < argc) {
                workerPort = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
            else if (arg == "--help") {
                std::cout << "Usage:\n";
                std::cout << "  engine\n";
                std::cout << "  engine --worker-port <port> [--worker-bind <host>]\n";
                return EXIT_SUCCESS;
            }
            else {
                throw std::invalid_argument("Unknown argument: " + std::string(arg));
            }
        }

        if (workerPort != 0)
            return engine::runDistributedWorkerServer(workerBindHost, workerPort);

        engine::UCIEngine uci;
        uci.loop();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return 0;
}
