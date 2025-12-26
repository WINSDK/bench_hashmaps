#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <absl/container/flat_hash_map.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

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
    uint64_t sum = 0;
    uint64_t steps = 0;
    size_t offset = 0;

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
    size_t iters) {
    boost::unordered::unordered_flat_map<uint64_t, uint64_t> map{};
    map.reserve(keys.size() * 2);
    for (const auto key : keys) {
        map.emplace(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iters;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iters, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                assert(it != map.end());
                return it->second;
            });

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_twoway(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iters) {
    TwoWay<detail::U64ToU64TableTrait, 4> twoway{};
    for (const auto key : keys) {
        twoway.insert(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iters;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iters, [&](uint64_t key, uint64_t* steps) {
                return twoway.find(key, steps);
            });

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_absl_flat_hash_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iters) {
    absl::flat_hash_map<uint64_t, uint64_t> map{};
    map.reserve(keys.size() * 2);
    for (const auto key : keys) {
        map.emplace(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iters;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iters, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                assert(it != map.end());
                return it->second;
            });

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_std_unordered_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iters) {
    std::unordered_map<uint64_t, uint64_t> map{};
    map.reserve(keys.size() * 2);
    for (const auto key : keys) {
        map.emplace(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        const auto lookup_count = lookups.size();
        const auto batch_size = lookup_count / iters;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iters, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                assert(it != map.end());
                return it->second;
            });

        results.emplace_back(counter, sum);
    }

    return results;
}

inline std::vector<BenchResult> benchmark_std_flat_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iters) {
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
        const auto batch_size = lookup_count / iters;

        auto [counter, sum] =
            benchmark_batch(lookups, batch_size, iters, [&](uint64_t key, uint64_t*) {
                const auto it = map.find(key);
                assert(it != map.end());
                return it->second;
            });

        results.emplace_back(counter, sum);
    }

    return results;
}
