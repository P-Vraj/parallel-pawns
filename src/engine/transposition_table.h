#pragma once

#include <algorithm>
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
    Eval score{};
    Move bestMove;
    Bound bound{};
    uint8_t depth{};
    uint8_t age{};
};
static_assert(sizeof(TTEntry) == 16);

class TranspositionTable {
public:
    TranspositionTable() noexcept = default;
    explicit TranspositionTable(size_t sizeMB = 64) noexcept {
        assert(sizeMB == 1 || sizeMB % 2 == 0);  // Must be a power of two
        resize(sizeMB);
    }
    void resize(size_t sizeMB) noexcept {
        const size_t bytes = sizeMB * 1024 * 1024;
        size_ = bytes / sizeof(TTEntry);
        table_.resize(size_);
        clear();
    }
    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return table_.empty(); }
    void clear() noexcept { std::ranges::fill(table_.begin(), table_.end(), TTEntry{}); }
    void newSearch() noexcept { ++age_; }
    const TTEntry* probe(Key key) const noexcept {
        if (empty())
            return nullptr;

        const TTEntry& entry = table_[index_(key)];
        if (entry.hash != key)
            return nullptr;

        return &entry;
    }
    void store(Key key, Move move, Eval score, uint8_t depth, Bound bound, int ply) noexcept {
        if (empty())
            return;

        TTEntry& entry = table_[index_(key)];
        const bool replace = (entry.hash == 0 || entry.hash == key || entry.age != age_ || depth >= entry.depth - 2);

        if (!replace)
            return;

        score = encodeMateScore(score, ply);
        entry = TTEntry{ key, score, move, bound, depth, age_ };
    }

private:
    size_t index_(Key key) const noexcept { return key & (size_ - 1); }

    std::vector<TTEntry> table_;
    size_t size_{};
    uint8_t age_{};
};