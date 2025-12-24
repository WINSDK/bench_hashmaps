#include "boost_improve.h"
#include "boost_unordered_improve.hpp"

#include <span>
#include <vector>

std::vector<BenchResult> benchmark_boost_improve(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations) {
    boost::unordered::unordered_flat_map<uint64_t, uint64_t> map{};
    map.reserve(keys.size() * 2);
    for (const auto key : keys) {
        map.emplace(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iterations;

        auto [ticks, sum] =
            benchmark_batch(lookups, batch_size, iterations, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                return it->second;
            });

        auto per_lookup = static_cast<double>(ticks) / static_cast<double>(lookup_count);
        results.emplace_back(per_lookup, sum);
    }

    return results;
}
