#pragma once

#include "bench_common.hpp"

#include <span>
#include <vector>

std::vector<BenchResult> benchmark_boost_baseline(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations);
