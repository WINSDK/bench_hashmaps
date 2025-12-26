#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <cstdint>
#include <format>
#include <print>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "bench.hpp"

constexpr auto ITERS = 100'000ULL;
constexpr std::array<size_t, 8> BATCH_SIZE{1, 2, 4, 8, 16, 32, 64, 128};
constexpr std::array<size_t, 5> NUM_KEYS_SHIFT{8, 10, 12, 14, 16};

namespace {
struct BenchSet {
    size_t shift;
    std::vector<BenchResult> boost;
    std::vector<BenchResult> twoway;
    std::vector<BenchResult> absl;
    std::vector<BenchResult> std_map;
    std::vector<BenchResult> flat;
};

std::vector<uint64_t> make_keys(uint64_t num_keys) {
    std::vector<uint64_t> keys{};
    keys.reserve(num_keys);
    for (uint64_t i = 0; i < num_keys; i++) {
        keys.emplace_back(i + 1);
    }
    return keys;
}

std::vector<std::vector<uint64_t>> make_random_lookup_sets(
    std::span<const uint64_t> keys,
    std::mt19937_64& rng) {
    std::uniform_int_distribution<size_t> dist{0, keys.size() - 1};

    std::vector<std::vector<uint64_t>> lookup_sets{};
    lookup_sets.reserve(BATCH_SIZE.size());
    for (const auto batch_size : BATCH_SIZE) {
        std::vector<uint64_t> lookups{};
        const auto lookup_count = ITERS * batch_size;
        lookups.reserve(lookup_count);
        for (size_t i = 0; i < lookup_count; i++) {
            lookups.emplace_back(keys[dist(rng)]);
        }
        lookup_sets.emplace_back(std::move(lookups));
    }

    return lookup_sets;
}

std::vector<std::vector<uint64_t>> make_dense_lookup_sets(std::span<const uint64_t> keys) {
    std::vector<std::vector<uint64_t>> lookup_sets{};
    lookup_sets.reserve(BATCH_SIZE.size());
    for (const auto batch_size : BATCH_SIZE) {
        std::vector<uint64_t> lookups{};
        const auto lookup_count = ITERS * batch_size;
        lookups.reserve(lookup_count);
        for (size_t i = 0; i < lookup_count; i++) {
            lookups.emplace_back(keys[i % keys.size()]);
        }
        lookup_sets.emplace_back(std::move(lookups));
    }

    return lookup_sets;
}

BenchSet run_benchmarks(
    size_t shift,
    std::span<const uint64_t> keys,
    std::span<const std::vector<uint64_t>> lookup_sets) {
    return BenchSet{
        shift,
        benchmark_boost(keys, lookup_sets, ITERS),
        benchmark_twoway(keys, lookup_sets, ITERS),
        benchmark_absl_flat_hash_map(keys, lookup_sets, ITERS),
        benchmark_std_unordered_map(keys, lookup_sets, ITERS),
        benchmark_std_flat_map(keys, lookup_sets, ITERS),
    };
}

void sink_results(const std::vector<BenchResult>& results) {
    for (const auto& result : results) {
        const auto sum = result.sum;
        write(open("/dev/null", O_WRONLY), &sum, sizeof(sum));
    }
}

void sink_all(const BenchSet& set) {
    sink_results(set.boost);
    sink_results(set.twoway);
    sink_results(set.absl);
    sink_results(set.std_map);
    sink_results(set.flat);
}

void print_table(const char* title, const std::vector<BenchSet>& results) {
    std::println("{}", title);
    std::println(
        "{:>10} {:>6} {:>10} {:>10} {:>10} {:>10} {:>10}",
        "N",
        "batch",
        "boost",
        "twoway",
        "absl",
        "std",
        "flat");
    for (const auto& entry : results) {
        for (size_t idx = 0; idx < BATCH_SIZE.size(); idx++) {
            const auto batch_size = BATCH_SIZE[idx];
            std::println(
                "{:>10} {:>6} {:>10} {:>10} {:>10} {:>10} {:>10}",
                std::format("1 << {}", entry.shift),
                batch_size,
                entry.boost[idx].avg_counter.cycles,
                entry.twoway[idx].avg_counter.cycles,
                entry.absl[idx].avg_counter.cycles,
                entry.std_map[idx].avg_counter.cycles,
                entry.flat[idx].avg_counter.cycles);
        }
    }

    fflush(stdout);
}
} // namespace

int main() {
    std::vector<BenchSet> random_results{};
    std::vector<BenchSet> dense_results{};
    random_results.reserve(NUM_KEYS_SHIFT.size());
    dense_results.reserve(NUM_KEYS_SHIFT.size());

    for (auto shift : NUM_KEYS_SHIFT) {
        const auto num_keys = 1ULL << shift;
        auto keys = make_keys(num_keys);

        std::mt19937_64 rng{0xC0FFEE ^ num_keys};
        auto lookup_sets = make_random_lookup_sets(keys, rng);
        auto dense_lookup_sets = make_dense_lookup_sets(keys);

        auto random_set = run_benchmarks(shift, keys, lookup_sets);
        auto dense_set = run_benchmarks(shift, keys, dense_lookup_sets);

        sink_all(random_set);
        sink_all(dense_set);

        random_results.emplace_back(std::move(random_set));
        dense_results.emplace_back(std::move(dense_set));
    }

    print_table("lookup performance (in cycles):", random_results);
    print_table("dense lookup (in cycles):", dense_results);

    return 0;
}
