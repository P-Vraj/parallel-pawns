#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "position.h"
#include "search.h"
#include "types.h"

namespace engine {

struct DistributedWorkerEndpoint {
    std::string host;
    uint16_t port{};
};

struct DistributedWorkerReport {
    DistributedWorkerEndpoint endpoint;
    std::vector<Move> assignedRootMoves;
    SearchResult result;
    bool usedRemote{false};
    bool fallbackToLocal{false};
};

bool parseDistributedWorkerEndpoints(
    std::string_view rawEndpoints,
    std::vector<DistributedWorkerEndpoint>& endpoints,
    std::string& error
);

SearchResult runDistributedRootSplitSearch(
    const Position& root,
    const SearchLimits& limits,
    const std::vector<Key>& positionHistory,
    size_t hashMb,
    SearchSharedState* sharedState,
    const std::vector<DistributedWorkerEndpoint>& endpoints,
    std::vector<DistributedWorkerReport>* reports = nullptr
);

int runDistributedWorkerServer(std::string_view bindHost, uint16_t port);

}  // namespace engine
