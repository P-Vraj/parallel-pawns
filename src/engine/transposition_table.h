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
    void resize(size_t sizeMB) {
        // Round size down to nearest power of two for efficient indexing
        const size_t roundedMB = std::max(std::bit_floor(sizeMB), static_cast<size_t>(1));
        size_ = roundedMB * 1024 * 1024 / sizeof(TTEntry);
        table_.resize(size_);
        clear();
    }
    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    void clear() noexcept {
        std::ranges::fill(table_.begin(), table_.end(), TTEntry{});
        resetCounters();
    }
    void newSearch() noexcept { ++age_; }
    const TTEntry* probe(Key key) const noexcept {
        if (empty()) {
            ++misses_;
            return nullptr;
        }

        const TTEntry& entry = table_[index_(key)];
        if (entry.hash != key) {
            ++misses_;
            return nullptr;
        }

        ++hits_;
        return &entry;
    }
    void store(Key key, Move move, TTScore score, uint8_t depth, Bound bound, int ply) noexcept {
        if (empty())
            return;

        TTEntry& entry = table_[index_(key)];
        const bool noEntry = (entry.hash == 0);
        const bool replace = (entry.hash == key || entry.age != age_ || depth >= entry.depth - 2);

        if (noEntry)
            ++writes_;
        else if (replace)
            ++rewrites_;
        else
            return;

        score = pack_TTScore(encode_mate_score(score, ply));
        entry = TTEntry{ key, score, move, bound, depth, age_ };
    }

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