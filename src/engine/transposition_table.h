#pragma once

#include <atomic>
#include <optional>
#include <vector>

#include "types.h"

enum class Bound : uint8_t {
    None,
    Exact,
    Lower,
    Upper,

    Count = 4
};

struct TTEntry {
    Key hash{};
    TTScore score{};
    Move bestMove;
    Bound bound{};
    uint8_t depth{};
    uint8_t age{};
};
static_assert(sizeof(TTEntry) == 16);

// A TTEntry is packed into 128 bits as follows:
// Bits 0-63:       Hash key
// Bits 64-79:      Score
// Bits 80-95:      Best move
// Bits 96-103:     Bound
// Bits 104-111:    Depth
// Bits 112-119:    Age
using PackedTTEntry = __uint128_t;

class TranspositionTable {
public:
    TranspositionTable() noexcept = default;
    explicit TranspositionTable(size_t sizeMB) { resize(sizeMB); }
    void resize(size_t sizeMB);
    void clear() noexcept;
    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    void newSearch() noexcept { ++age_; }

    std::optional<TTEntry> probe(Key key) const noexcept;
    void store(Key key, Move move, TTScore score, uint8_t depth, Bound bound, int ply) noexcept;

    // Statistics
    size_t hits() const noexcept { return hits_; }
    size_t misses() const noexcept { return misses_; }
    size_t writes() const noexcept { return writes_; }
    size_t rewrites() const noexcept { return rewrites_; }
    float hitRate() const noexcept {
        const size_t totalAccesses = hits_ + misses_;
        return (totalAccesses > 0) ? static_cast<float>(hits_) / static_cast<float>(totalAccesses) : 0.0F;
    }
    float rewriteRate() const noexcept {
        const size_t totalWrites = writes_ + rewrites_;
        return (totalWrites > 0) ? static_cast<float>(rewrites_) / static_cast<float>(totalWrites) : 0.0F;
    }
    void resetCounters() noexcept {
        hits_ = 0;
        misses_ = 0;
        writes_ = 0;
        rewrites_ = 0;
    }

private:
    size_t index_(Key key) const noexcept { return key & (size_ - 1); }

    static TTEntry unpack_(PackedTTEntry packed) noexcept;
    static PackedTTEntry pack_(Key key, TTScore score, Move bestMove, Bound bound, uint8_t depth, uint8_t age) noexcept;

    std::vector<std::atomic<PackedTTEntry>> table_;
    size_t size_{};
    std::atomic<uint8_t> age_{0};
    mutable std::atomic<size_t> hits_{0};
    mutable std::atomic<size_t> misses_{0};
    mutable std::atomic<size_t> writes_{0};
    mutable std::atomic<size_t> rewrites_{0};
};
