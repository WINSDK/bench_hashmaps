#pragma once

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "TwoWay.hpp"
#include "boost_unordered.hpp"
#include "measure.hpp"

struct BenchResult {
    PerfCounters counter;
    uint64_t sum;
};

inline std::tuple<PerfCounters, uint64_t> benchmark_batch(
    std::span<const uint64_t> lookups,
    size_t batch_size,
    size_t iters,
    auto&& lookup_fn) {
    auto sum = 0ULL;
    auto steps = 0ULL;
    auto offset = 0ULL;

    const auto start = RECORDER.get_counters();
    for (size_t iter = 0; iter < iters; iter++) {
        const auto* batch = &lookups[offset];
        offset += batch_size;
        for (size_t i = 0; i < batch_size; i++) {
            sum += lookup_fn(batch[i], &steps);
        }
    }
    const auto end = RECORDER.get_counters();
    return {(end - start) / lookups.size(), sum};
}

namespace detail {
struct U64ToU64TableTrait {
    using Key = uint64_t;
    using Value = uint64_t;

    static uint64_t hash(Key key) {
        return squirrel3(key);
    }
};
} // namespace detail

inline std::vector<BenchResult> benchmark_boost(
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

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iterations, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                return it->second;
            });

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_twoway(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations) {
    TwoWay<detail::U64ToU64TableTrait, 4> twoway{};
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

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_absl_flat_hash_map(
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

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_std_unordered_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations) {
    std::unordered_map<uint64_t, uint64_t> map{};
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

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_std_flat_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations) {
    std::vector<std::pair<uint64_t, uint64_t>> items{};
    items.reserve(keys.size());
    for (const auto key : keys) {
        items.emplace_back(key, key);
    }

    std::flat_map<uint64_t, uint64_t> map{std::sorted_unique, items.begin(), items.end()};

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

        results.emplace_back(counter, sum);
    }

    return results;
}
