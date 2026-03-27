#pragma once

#include <algorithm>
#include <bit>
#include <memory>
#include <vector>

#include "eval_constants.h"
#include "position.h"
#include "types.h"
#include "zobrist.h"

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

class TranspositionTable {
public:
    TranspositionTable() noexcept = default;
    explicit TranspositionTable(size_t sizeMB) { resize(sizeMB); }
    void resize(size_t sizeMB);
    void clear() noexcept;
    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    void newSearch() noexcept { ++age_; }
    
    const TTEntry* probe(Key key) const noexcept;
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

    std::vector<TTEntry> table_;
    size_t size_{};
    uint8_t age_{};
    mutable size_t hits_{};
    mutable size_t misses_{};
    mutable size_t writes_{};
    mutable size_t rewrites_{};
};
