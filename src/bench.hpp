#pragma once

#include <algorithm>
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
#include "dynamic_fph_table.hpp"
#include "measure.hpp"

struct BenchResult {
    PerfCounters counter;
    uint64_t sum;
    uint64_t lookups;
};

inline std::tuple<PerfCounters, uint64_t> benchmark_batch(
    std::span<const uint64_t> lookups,
    size_t batch_size,
    size_t iters,
    auto&& lookup_fn,
    PerfCounterSet counter_set,
    size_t warmup_iters = 1) {
    uint64_t sum = 0;
    uint64_t steps = 0;
    size_t offset = 0;

    if (warmup_iters > 0) {
        size_t warmup_rounds = std::min(warmup_iters, iters);
        size_t warmup_offset = 0;
        volatile uint64_t warmup_sum = 0;
        uint64_t warmup_steps = 0;
        for (size_t iter = 0; iter < warmup_rounds; iter++) {
            const auto* batch = &lookups[warmup_offset];
            warmup_offset += batch_size;
            for (size_t i = 0; i < batch_size; i++) {
                warmup_sum += lookup_fn(batch[i], &warmup_steps);
            }
        }
    }

    RECORDER.disable_all();
    RECORDER.enable(counter_set);
    const auto start = RECORDER.get_counters(counter_set);
    for (size_t iter = 0; iter < iters; iter++) {
        const auto* batch = &lookups[offset];
        offset += batch_size;
        for (size_t i = 0; i < batch_size; i++) {
            sum += lookup_fn(batch[i], &steps);
        }
    }
    const auto end = RECORDER.get_counters(counter_set);
    RECORDER.disable_all();
    return {end - start, sum};
}

inline BenchResult benchmark_split(
    std::span<const uint64_t> lookups,
    size_t iters,
    auto&& lookup_fn) {
    const auto batch_size = lookups.size() / iters;
    auto [counter, sum] =
        benchmark_batch(lookups, batch_size, iters, lookup_fn, PerfCounterSet::core);
#if defined(__linux__)
    auto [cache_counter, cache_sum] =
        benchmark_batch(lookups, batch_size, iters, lookup_fn, PerfCounterSet::cache);
    counter.l1d_accesses = cache_counter.l1d_accesses;
    counter.l1d_misses = cache_counter.l1d_misses;
    counter.l1d_time_enabled = cache_counter.l1d_time_enabled;
    counter.l1d_time_running = cache_counter.l1d_time_running;
    counter.llc_accesses = cache_counter.llc_accesses;
    counter.llc_misses = cache_counter.llc_misses;
    counter.llc_time_enabled = cache_counter.llc_time_enabled;
    counter.llc_time_running = cache_counter.llc_time_running;
    sum += cache_sum;
#endif
    return {counter, sum, lookups.size()};
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
        results.emplace_back(benchmark_split(lookups, iters, [&](uint64_t key, uint64_t*) {
            const auto it = map.find(key);
            assert(it != map.end());
            return it->second;
        }));
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
        results.emplace_back(benchmark_split(lookups, iters, [&](uint64_t key, uint64_t* steps) {
            return twoway.find(key, steps);
        }));
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
        results.emplace_back(benchmark_split(lookups, iters, [&](uint64_t key, uint64_t*) {
            const auto it = map.find(key);
            assert(it != map.end());
            return it->second;
        }));
    }

    return results;
}

inline std::vector<BenchResult> benchmark_dynamic_fph_map(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iters) {
    fph::DynamicFphMap<uint64_t, uint64_t> map{};
    map.reserve(keys.size() * 2);
    for (const auto key : keys) {
        map.emplace(key, key);
    }

    auto results = std::vector<BenchResult>{};
    results.reserve(lookup_sets.size());

    for (const auto& lookups : lookup_sets) {
        results.emplace_back(benchmark_split(lookups, iters, [&](uint64_t key, uint64_t*) {
            const auto it = map.find(key);
            assert(it != map.end());
            return it->second;
        }));
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
        results.emplace_back(benchmark_split(lookups, iters, [&](uint64_t key, uint64_t*) {
            const auto it = map.find(key);
            assert(it != map.end());
            return it->second;
        }));
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
        results.emplace_back(benchmark_split(lookups, iters, [&](uint64_t key, uint64_t*) {
            const auto it = map.find(key);
            assert(it != map.end());
            return it->second;
        }));
    }

    return results;
}
