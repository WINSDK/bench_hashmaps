#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <cstdint>
#include <print>
#include <random>
#include <span>
#include <vector>

#include "TwoWay.hpp"
#include "base.hpp"
#include "bench.hpp"
#include "boost_baseline.hpp"
#include "boost_improve.hpp"
#include "measure.hpp"

struct U64ToU64TableTrait {
    using Key = uint64_t;
    using Value = uint64_t;

    static uint64_t hash(Key key) {
        return squirrel3(key);
    }
};

constexpr auto ITERS = 100'000ULL;
constexpr std::array<size_t, 8> BATCH_SIZE{1, 2, 4, 8, 16, 32, 64, 128};
constexpr std::array<size_t, 5> NUM_KEYS_SHIFT{8, 10, 12, 14, 16};

int main() {
    std::println("lookup performance (in cycles):");
    std::println("{:>10} {:>6} {:>10} {:>10} {:>10}", "N", "batch", "baseline", "twoway", "boost");

    for (auto shift : NUM_KEYS_SHIFT) {
        const auto num_keys = 1ULL << shift;

        std::vector<uint64_t> keys{};
        keys.reserve(num_keys);
        for (uint64_t i = 0; i < num_keys; i++) {
            keys.emplace_back(i + 1);
        }

        std::mt19937_64 rng{0xC0FFEE ^ num_keys};
        std::uniform_int_distribution<size_t> dist{0, keys.size() - 1};

        std::vector<std::vector<uint64_t>> lookup_sets{};
        lookup_sets.reserve(BATCH_SIZE.size());
        for (const auto batch_size : BATCH_SIZE) {
            auto lookups = std::vector<uint64_t>{};
            const auto lookup_count = ITERS * batch_size;
            lookups.reserve(lookup_count);
            for (size_t i = 0; i < lookup_count; i++) {
                lookups.emplace_back(keys[dist(rng)]);
            }
            lookup_sets.emplace_back(std::move(lookups));
        }

        const auto baseline_results = benchmark_boost_baseline(keys, lookup_sets, ITERS);
        const auto improve_results = benchmark_boost_improve(keys, lookup_sets, ITERS);

        auto twoway = TwoWay<U64ToU64TableTrait, 4>{};
        for (const auto key : keys) {
            twoway.insert(key, key);
        }

        auto baseline_sum = 0ULL;
        auto twoway_sum = 0ULL;
        auto boost_sum = 0ULL;

        for (size_t idx = 0; idx < BATCH_SIZE.size(); idx++) {
            const auto batch_size = BATCH_SIZE[idx];
            const auto& lookups = lookup_sets[idx];
            const auto total_ops = ITERS * batch_size;
            baseline_sum = baseline_results[idx].sum;

            const auto [counter, twoway_lookup_sum] =
                benchmark_batch(lookups, batch_size, ITERS, [&](uint64_t key, uint64_t* steps) {
                    return twoway.find(key, steps);
                });
            twoway_sum = twoway_lookup_sum;

            const auto twoway_per_lookup =
                static_cast<double>(counter.cycles) / static_cast<double>(total_ops);

            boost_sum = improve_results[idx].sum;
            const auto boost_per_lookup = improve_results[idx].ticks_per_lookup;

            std::println(
                "{:>10} {:>6} {:>10.2f} {:>10.2f} {:>10.2f}",
                std::format("1 << {}", shift),
                batch_size,
                baseline_results[idx].ticks_per_lookup,
                twoway_per_lookup,
                boost_per_lookup);
        }

        // Write sum to /dev/null to break to prevent optimizing intermediate results.
        for (const auto sum : {baseline_sum, twoway_sum, boost_sum}) {
            write(open("/dev/null", O_WRONLY), &sum, sizeof(sum));
        }
    }

    return 0;
}
