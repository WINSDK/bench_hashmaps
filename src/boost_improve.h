#pragma once

#include "bench.h"

#include <span>
#include <vector>

std::vector<BenchResult> benchmark_boost_improve(
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets,
    size_t iterations);
