#include <array>
#include <cstdint>
#include <print>
#include <random>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#include "base.h"
#include "boost_unordered.hpp"
#include "two_way.h"

struct U64ToU64TableTrait {
    using Key = uint64_t;
    using Value = uint64_t;

    static uint64_t hash(Key key) {
        return squirrel3(key);
    }
};

uint64_t benchmark_batch(
    const std::vector<uint64_t>& lookups,
    size_t batch_size,
    size_t iterations,
    auto&& lookup_fn,
    uint64_t* sum_out) {
    auto sum = 0ULL;
    auto steps = 0ULL;
    const auto* data = lookups.data();
    auto offset = size_t{0};

    const auto start = __rdtsc();
    for (size_t iter = 0; iter < iterations; iter++) {
        const auto* batch = data + offset;
        offset += batch_size;
        for (size_t i = 0; i < batch_size; i++) {
            sum += lookup_fn(batch[i], &steps);
        }
    }
    const auto end = __rdtsc();
    *sum_out = sum;
    return end - start;
}

int main() {
    std::println("lookup performance (in cycles):");
    std::println("N\tbatch\tbaseline\timprove\tboost");
    constexpr auto kIterations = size_t{100000};
    constexpr auto kBatchSizes = std::array<size_t, 8>{1, 2, 4, 8, 16, 32, 64, 128};
    constexpr auto kNumKeysList = std::array<size_t, 3>{1 << 18, 1 << 21, 1 << 23};

    for (const auto num_keys : kNumKeysList) {
        auto keys = std::vector<uint64_t>{};
        keys.reserve(num_keys);
        for (uint64_t i = 0; i < num_keys; i++) {
            keys.push_back(i + 1);
        }

        TwoWayBaseline<U64ToU64TableTrait, 4> baseline;
        TwoWay<U64ToU64TableTrait, 4> candidate;
        boost::unordered::unordered_flat_map<uint64_t, uint64_t> boost_map;
        boost_map.reserve(num_keys * 2);

        for (const auto key : keys) {
            baseline.insert(key, key);
            candidate.insert(key, key);
            boost_map.emplace(key, key);
        }

        auto rng = std::mt19937_64{0xC0FFEE ^ num_keys};
        auto dist = std::uniform_int_distribution<size_t>{0, keys.size() - 1};

        auto lookup_sets = std::vector<std::vector<uint64_t>>{};
        lookup_sets.reserve(kBatchSizes.size());
        for (const auto batch_size : kBatchSizes) {
            auto lookups = std::vector<uint64_t>{};
            const auto lookup_count = kIterations * batch_size;
            lookups.reserve(lookup_count);
            for (size_t i = 0; i < lookup_count; i++) {
                lookups.push_back(keys[dist(rng)]);
            }
            lookup_sets.emplace_back(std::move(lookups));
        }

        auto baseline_sum = 0ULL;
        auto candidate_sum = 0ULL;
        auto boost_sum = 0ULL;

        for (size_t idx = 0; idx < kBatchSizes.size(); idx++) {
            const auto batch_size = kBatchSizes[idx];
            const auto& lookups = lookup_sets[idx];
            const auto total_ops = kIterations * batch_size;

            const auto baseline_cycles = benchmark_batch(
                lookups,
                batch_size,
                kIterations,
                [&](uint64_t key, uint64_t* steps) { return baseline.find(key, steps); },
                &baseline_sum);

            const auto candidate_cycles = benchmark_batch(
                lookups,
                batch_size,
                kIterations,
                [&](uint64_t key, uint64_t* steps) { return candidate.find(key, steps); },
                &candidate_sum);

            const auto boost_cycles = benchmark_batch(
                lookups,
                batch_size,
                kIterations,
                [&](uint64_t key, uint64_t*) {
                    const auto it = boost_map.find(key);
                    return it->second;
                },
                &boost_sum);

            const auto baseline_per_lookup =
                static_cast<double>(baseline_cycles) / static_cast<double>(total_ops);
            const auto candidate_per_lookup =
                static_cast<double>(candidate_cycles) / static_cast<double>(total_ops);
            const auto boost_per_lookup =
                static_cast<double>(boost_cycles) / static_cast<double>(total_ops);

            std::println(
                "{}\t{}\t{:.2f}\t{:.2f}\t{:.2f}",
                num_keys,
                batch_size,
                baseline_per_lookup,
                candidate_per_lookup,
                boost_per_lookup);
        }

        // Write sum to /dev/null to break to prevent optimizing intermediate results. 
        for (const auto sum : {baseline_sum, candidate_sum, boost_sum}) {
            write(open("/dev/null", O_WRONLY), &sum, sizeof(sum));
        }
    }

    return 0;
}
