#include "bench_absl.hpp"

#include <absl/container/flat_hash_map.h>

#include <span>
#include <vector>

std::vector<BenchResult> benchmark_absl_flat_hash_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations) {
    absl::flat_hash_map<uint64_t, uint64_t> map{};
    map.reserve(keys.size() * 2);
    for (const auto key : keys) {
        map.emplace(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iterations;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iterations, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                return it->second;
            });

        auto per_lookup = static_cast<double>(counter.cycles) / static_cast<double>(lookup_count);
        results.emplace_back(per_lookup, sum);
    }

    return results;
}
