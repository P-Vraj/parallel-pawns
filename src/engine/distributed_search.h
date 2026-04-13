#pragma once

#include <cstdint>
#include <memory>
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
    int threads{1};
};

struct DistributedWorkerReport {
    DistributedWorkerEndpoint endpoint;
    std::vector<Move> assignedRootMoves;
    SearchResult result;
    bool coordinatorParticipant{false};
    bool usedRemote{false};
    bool fallbackToLocal{false};
    bool sessionReused{false};
    int sessionRequestCount{0};
    int sessionReconnectCount{0};
    uint64_t roundTripMs{0};
    uint64_t overheadMs{0};
};

class DistributedCoordinatorSessions {
public:
    DistributedCoordinatorSessions();
    ~DistributedCoordinatorSessions();
    DistributedCoordinatorSessions(const DistributedCoordinatorSessions&) = delete;
    DistributedCoordinatorSessions& operator=(const DistributedCoordinatorSessions&) = delete;
    DistributedCoordinatorSessions(DistributedCoordinatorSessions&&) noexcept;
    DistributedCoordinatorSessions& operator=(DistributedCoordinatorSessions&&) noexcept;

    void reset();
    void setEndpoints(const std::vector<DistributedWorkerEndpoint>& endpoints);

private:
    friend SearchResult runDistributedRootSplitSearch(
        const Position& root,
        const SearchLimits& limits,
        const std::vector<Key>& positionHistory,
        size_t hashMb,
        SearchSharedState* sharedState,
        const std::vector<DistributedWorkerEndpoint>& endpoints,
        DistributedCoordinatorSessions* sessions,
        std::vector<DistributedWorkerReport>* reports
    );
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool parseDistributedWorkerEndpoints(
    std::string_view rawEndpoints,
    std::vector<DistributedWorkerEndpoint>& endpoints,
    std::string& error
);
bool parseDistributedWorkerConfigFile(
    std::string_view path,
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
    DistributedCoordinatorSessions* sessions = nullptr,
    std::vector<DistributedWorkerReport>* reports = nullptr
);

int runDistributedWorkerServer(std::string_view bindHost, uint16_t port);

}  // namespace engine
