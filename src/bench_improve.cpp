#include "bench_improve.hpp"
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

        auto sum = 0ULL;
        auto offset = 0ULL;
        auto iters = std::vector<decltype(map.begin())>(batch_size);

        const auto start = RECORDER.get_counters();
        for (size_t iter = 0; iter < iterations; iter++) {
            const auto* batch = &lookups[offset];
            offset += batch_size;
            map.findMany(batch, batch + batch_size, iters.begin());
            for (size_t i = 0; i < batch_size; i++) {
                sum += iters[i]->second;
            }
        }
        const auto end = RECORDER.get_counters();
        auto counter = end - start;

        auto per_lookup = static_cast<double>(counter.cycles) / static_cast<double>(lookup_count);
        results.emplace_back(per_lookup, sum);
    }

    return results;
}
