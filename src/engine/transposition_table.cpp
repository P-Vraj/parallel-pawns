#include "transposition_table.h"

#include <algorithm>
#include <bit>
#include <memory>

#include "eval_constants.h"

void TranspositionTable::resize(size_t sizeMB) {
    // Round size down to nearest power of two for efficient indexing
    const size_t roundedMB = std::max(std::bit_floor(sizeMB), static_cast<size_t>(1));
    size_ = roundedMB * 1024 * 1024 / sizeof(std::atomic<PackedTTEntry>);
    table_ = std::vector<std::atomic<PackedTTEntry>>(size_);
    clear();
}

void TranspositionTable::clear() noexcept {
    for (std::atomic<PackedTTEntry>& entry : table_) {
        entry.store(static_cast<PackedTTEntry>(0), std::memory_order_relaxed);
    }
    resetCounters();
}

std::optional<TTEntry> TranspositionTable::probe(Key key) const noexcept {
    if (empty()) {
        ++misses_;
        return std::nullopt;
    }

    const PackedTTEntry entry = table_[index_(key)].load(std::memory_order_relaxed);
    const TTEntry unpackedEntry = unpack_(entry);

    if (unpackedEntry.hash != key) {
        ++misses_;
        return std::nullopt;
    }

    ++hits_;
    return std::make_optional(unpackedEntry);
}

void TranspositionTable::store(Key key, Move move, TTScore score, uint8_t depth, Bound bound, int ply) noexcept {
    if (empty())
        return;

    std::atomic<PackedTTEntry>& slot = table_[index_(key)];
    const uint8_t age = age_.load(std::memory_order_relaxed);

    const TTEntry entry = unpack_(slot.load(std::memory_order_relaxed));
    const bool noEntry = (entry.hash == 0);
    const bool replace = (entry.hash == key || entry.age != age || depth >= entry.depth - 2);

    if (noEntry)
        ++writes_;
    else if (replace)
        ++rewrites_;
    else
        return;

    score = pack_TTScore(encode_mate_score(score, ply));
    slot.store(pack_(key, score, move, bound, depth, age), std::memory_order_relaxed);
}

TTEntry TranspositionTable::unpack_(PackedTTEntry packed) noexcept {
    return TTEntry{
        .hash = static_cast<Key>(packed & 0xFFFFFFFFFFFFFFFF),
        .score = static_cast<TTScore>((packed >> 64) & 0xFFFF),
        .bestMove = Move(static_cast<uint16_t>((packed >> 80) & 0xFFFF)),
        .bound = static_cast<Bound>((packed >> 96) & 0xFF),
        .depth = static_cast<uint8_t>((packed >> 104) & 0xFF),
        .age = static_cast<uint8_t>((packed >> 112) & 0xFF)
    };
}

PackedTTEntry TranspositionTable::pack_(
    Key key,
    TTScore score,
    Move bestMove,
    Bound bound,
    uint8_t depth,
    uint8_t age
) noexcept {
    return (static_cast<PackedTTEntry>(static_cast<uint64_t>(key))) |
           (static_cast<PackedTTEntry>(static_cast<uint16_t>(score)) << 64) |
           (static_cast<PackedTTEntry>(bestMove.data()) << 80) |
           (static_cast<PackedTTEntry>(static_cast<uint8_t>(bound)) << 96) |
           (static_cast<PackedTTEntry>(depth) << 104) | (static_cast<PackedTTEntry>(age) << 112);
}
