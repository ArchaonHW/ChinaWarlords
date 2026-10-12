#include "Campaign/Chain/LedgerMerkle.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Potato::Campaign {

using Gameplay::Result;

namespace {

constexpr std::uint64_t kOffset = 14695981039346656037ull; // FNV-1a
constexpr std::uint64_t kPrime = 1099511628211ull;

// Domain bytes — a leaf and a node can never collide by
// construction: the first hashed byte differs by level.
constexpr unsigned char kNodeDomain = 0x00;
constexpr unsigned char kEntryDomain = 0x01;
constexpr std::string_view kClaimDomain("\x02", 1);

std::uint64_t Fnv1aByte(unsigned char b, std::uint64_t h) {
    h ^= b;
    return h * kPrime;
}

std::uint64_t Fnv1aBytes(std::string_view s, std::uint64_t h) {
    for (unsigned char c : s) {
        h = Fnv1aByte(c, h);
    }
    return h;
}

// LE expansion — the same little-endian convention EntryHash uses
// for prevHash.
std::uint64_t Fnv1aU64(std::uint64_t v, std::uint64_t h) {
    for (int i = 0; i < 8; ++i) {
        h = Fnv1aByte(static_cast<unsigned char>(v & 0xff), h);
        v >>= 8;
    }
    return h;
}

std::uint64_t EntryLeaf(const LedgerEntry& e) {
    return Fnv1aU64(e.hash, Fnv1aByte(kEntryDomain, kOffset));
}

std::uint64_t ClaimLeaf(const std::string& emit) {
    return Fnv1aBytes(emit, Fnv1aBytes(kClaimDomain, kOffset));
}

std::uint64_t Node(std::uint64_t left, std::uint64_t right) {
    std::uint64_t h = Fnv1aByte(kNodeDomain, kOffset);
    return Fnv1aU64(right, Fnv1aU64(left, h));
}

} // namespace

Result<std::uint64_t>
LedgerMerkleRoot(const Ledger& l, std::span<const MintClaim> claims) {
    // Canonical leaf list: entries in seq order, then claims sorted
    // by (ClaimId, canonical emit) — set semantics, input order is
    // irrelevant.
    std::vector<std::uint64_t> leaves;
    leaves.reserve(l.Size() + claims.size());
    for (const LedgerEntry& e : l.Entries()) {
        leaves.push_back(EntryLeaf(e));
    }

    std::vector<std::pair<std::string, std::string>> keyed;
    keyed.reserve(claims.size());
    for (const MintClaim& c : claims) {
        const auto doc = c.ToJson();
        if (!doc.ok()) {
            return Gameplay::Fail<std::uint64_t>(doc.error,
                                                 doc.reason);
        }
        keyed.emplace_back(ClaimId(c), doc.value.Emit());
    }
    std::sort(keyed.begin(), keyed.end());
    keyed.erase(std::unique(keyed.begin(), keyed.end()),
                keyed.end());
    for (const auto& [id, emit] : keyed) {
        leaves.push_back(ClaimLeaf(emit));
    }

    if (leaves.empty()) {
        return Gameplay::Ok<std::uint64_t>(0);
    }
    // Pair-wise fold; odd leaf promotes unpaired (RFC-6962 style).
    while (leaves.size() > 1) {
        std::vector<std::uint64_t> next;
        next.reserve((leaves.size() + 1) / 2);
        for (std::size_t i = 0; i < leaves.size(); i += 2) {
            if (i + 1 < leaves.size()) {
                next.push_back(Node(leaves[i], leaves[i + 1]));
            } else {
                next.push_back(leaves[i]);
            }
        }
        leaves = std::move(next);
    }
    return Gameplay::Ok(leaves[0]);
}

} // namespace Potato::Campaign
