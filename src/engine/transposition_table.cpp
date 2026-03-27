#include "transposition_table.h"

#include <algorithm>
#include <bit>
#include <memory>

#include "eval_constants.h"

void TranspositionTable::resize(size_t sizeMB) {
    // Round size down to nearest power of two for efficient indexing
    const size_t roundedMB = std::max(std::bit_floor(sizeMB), static_cast<size_t>(1));
    size_ = roundedMB * 1024 * 1024 / sizeof(TTEntry);
    table_.resize(size_);
    clear();
}

void TranspositionTable::clear() noexcept {
    std::ranges::fill(table_.begin(), table_.end(), TTEntry{});
    resetCounters();
}

const TTEntry* TranspositionTable::probe(Key key) const noexcept {
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

void TranspositionTable::store(Key key, Move move, TTScore score, uint8_t depth, Bound bound, int ply) noexcept {
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
    entry = TTEntry{key, score, move, bound, depth, age_};
}
