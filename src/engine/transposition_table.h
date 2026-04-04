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
    Key hash{};         // Hash of position
    TTScore score{};    // Score from this position
    Move bestMove;      // Best move from this position
    Bound bound{};      // Type of bound (exact, lower, upper)
    uint8_t depth{};    // Search depth at which this entry was stored (used for replacement)
    uint8_t age{};      // Age of the entry (used for replacement)
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

struct TTStats {
    uint64_t hits{};
    uint64_t misses{};
    uint64_t writes{};
    uint64_t rewrites{};
};

class TranspositionTable {
public:
    TranspositionTable() noexcept = default;
    explicit TranspositionTable(size_t sizeMB) { resize(sizeMB); }
    void resize(size_t sizeMB);
    void clear() noexcept;
    size_t size() const noexcept {
        assert(size_ == table_.size());
        return size_;
    }
    bool empty() const noexcept { return size_ == 0; }
    void newSearch() noexcept { ++age_; }

    std::optional<TTEntry> probe(Key key) const noexcept;
    void store(Key key, Move move, TTScore score, uint8_t depth, Bound bound, int ply) noexcept;

    // Statistics
    size_t hits() const noexcept { return hits_.load(std::memory_order_relaxed); }
    size_t misses() const noexcept { return misses_.load(std::memory_order_relaxed); }
    size_t writes() const noexcept { return writes_.load(std::memory_order_relaxed); }
    size_t rewrites() const noexcept { return rewrites_.load(std::memory_order_relaxed); }
    TTStats stats() const noexcept {
        return TTStats{
            .hits = hits_.load(std::memory_order_relaxed),
            .misses = misses_.load(std::memory_order_relaxed),
            .writes = writes_.load(std::memory_order_relaxed),
            .rewrites = rewrites_.load(std::memory_order_relaxed),
        };
    }
    float hitRate() const noexcept {
        const TTStats snapshot = stats();
        const uint64_t totalAccesses = snapshot.hits + snapshot.misses;
        return (totalAccesses > 0) ? static_cast<float>(snapshot.hits) / static_cast<float>(totalAccesses) : 0.0F;
    }
    float rewriteRate() const noexcept {
        const TTStats snapshot = stats();
        const uint64_t totalWrites = snapshot.writes + snapshot.rewrites;
        return (totalWrites > 0) ? static_cast<float>(snapshot.rewrites) / static_cast<float>(totalWrites) : 0.0F;
    }
    void resetCounters() noexcept {
        hits_.store(0, std::memory_order_relaxed);
        misses_.store(0, std::memory_order_relaxed);
        writes_.store(0, std::memory_order_relaxed);
        rewrites_.store(0, std::memory_order_relaxed);
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
