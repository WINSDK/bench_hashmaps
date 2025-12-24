#pragma once

#include "base.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>

struct BenchResult {
    double ticks_per_lookup;
    uint64_t sum;
};

inline std::tuple<uint64_t, uint64_t> benchmark_batch(
    std::span<const uint64_t> lookups,
    size_t batch_size,
    size_t iterations,
    auto&& lookup_fn) {
    auto sum = 0ULL;
    auto steps = 0ULL;
    const auto* data = lookups.data();
    auto offset = size_t{0};

    const auto start = read_ticks();
    for (size_t iter = 0; iter < iterations; iter++) {
        const auto* batch = data + offset;
        offset += batch_size;
        for (size_t i = 0; i < batch_size; i++) {
            sum += lookup_fn(batch[i], &steps);
        }
    }
    const auto end = read_ticks();
    return {end - start, sum};
}
