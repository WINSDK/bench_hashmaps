#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>

#include "measure.hpp"

struct BenchResult {
    double ticks_per_lookup;
    uint64_t sum;
};

inline std::tuple<performance_counters, uint64_t> benchmark_batch(
    std::span<const uint64_t> lookups,
    size_t batch_size,
    size_t iterations,
    auto&& lookup_fn) {
    auto sum = 0ULL;
    auto steps = 0ULL;
    auto offset = 0ULL;

    const auto start = RECORDER.get_counters();
    for (size_t iter = 0; iter < iterations; iter++) {
        const auto* batch = &lookups[offset];
        offset += batch_size;
        for (size_t i = 0; i < batch_size; i++) {
            sum += lookup_fn(batch[i], &steps);
        }
    }
    const auto end = RECORDER.get_counters();
    return {end - start, sum};
}
