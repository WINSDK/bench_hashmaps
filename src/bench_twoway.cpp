#include "bench_twoway.hpp"

#include "TwoWay.hpp"

#include <span>
#include <vector>

namespace {
struct U64ToU64TableTrait {
    using Key = uint64_t;
    using Value = uint64_t;

    static uint64_t hash(Key key) {
        return squirrel3(key);
    }
};
} // namespace

std::vector<BenchResult> benchmark_twoway(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations) {
    TwoWay<U64ToU64TableTrait, 4> twoway{};
    for (const auto key : keys) {
        twoway.insert(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iterations;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iterations, [&](uint64_t key, uint64_t* steps) {
                return twoway.find(key, steps);
            });

        auto per_lookup = static_cast<double>(counter.cycles) / static_cast<double>(lookup_count);
        results.emplace_back(per_lookup, sum);
    }

    return results;
}
