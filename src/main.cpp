#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <print>
#include <random>
#include <span>
#include <string>
#include <string_view>
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
    std::vector<BenchResult> fph;
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
        benchmark_dynamic_fph_map(keys, lookup_sets, ITERS),
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
    sink_results(set.fph);
    sink_results(set.std_map);
    sink_results(set.flat);
}

struct Table {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    std::vector<size_t> widths;
};

struct TableOutput {
    size_t shift;
    Table table;
};

std::string hit_rate_percent(uint64_t accesses, uint64_t misses) {
    if (accesses == 0) {
        return "na";
    }
    if (misses >= accesses) {
        return "0";
    }
    return std::format("{}", 100 - (misses * 100 / accesses));
}

uint64_t scale_counter(uint64_t count, uint64_t time_enabled, uint64_t time_running) {
    if (time_enabled == 0 || time_running == 0) {
        return count;
    }
    if (time_enabled == time_running) {
        return count;
    }
    const auto scaled = static_cast<__int128>(count) * time_enabled / time_running;
    return static_cast<uint64_t>(scaled);
}

std::string format_cell(const BenchResult& result) {
    const auto cycles = scale_counter(
        result.counter.cycles,
        result.counter.core_time_enabled,
        result.counter.core_time_running);
    const auto branches = scale_counter(
        result.counter.branches,
        result.counter.core_time_enabled,
        result.counter.core_time_running);
    const auto missed_branches = scale_counter(
        result.counter.missed_branches,
        result.counter.core_time_enabled,
        result.counter.core_time_running);
    const auto l1d_accesses = scale_counter(
        result.counter.l1d_accesses,
        result.counter.l1d_time_enabled,
        result.counter.l1d_time_running);
    const auto l1d_misses = scale_counter(
        result.counter.l1d_misses,
        result.counter.l1d_time_enabled,
        result.counter.l1d_time_running);
    const auto llc_accesses = scale_counter(
        result.counter.llc_accesses,
        result.counter.llc_time_enabled,
        result.counter.llc_time_running);
    const auto llc_misses = scale_counter(
        result.counter.llc_misses,
        result.counter.llc_time_enabled,
        result.counter.llc_time_running);

    const auto cycles_per_lookup = result.lookups == 0 ? 0 : cycles / result.lookups;
    const auto branch_hit = hit_rate_percent(branches, missed_branches);
    const auto l1d_hit = hit_rate_percent(l1d_accesses, l1d_misses);
    const auto llc_hit = hit_rate_percent(llc_accesses, llc_misses);
    return std::format("{}/{}/{}/{}", cycles_per_lookup, branch_hit, l1d_hit, llc_hit);
}

Table make_table(const BenchSet& set) {
    Table table;
    table.headers.emplace_back("kind");
    for (const auto batch_size : BATCH_SIZE) {
        table.headers.emplace_back(std::format("{}", batch_size));
    }

    struct RowSpec {
        std::string_view name;
        const std::vector<BenchResult> BenchSet::* member;
    };
    constexpr std::array<RowSpec, 6> kRows{{
        {"boost", &BenchSet::boost},
        {"twoway", &BenchSet::twoway},
        {"absl", &BenchSet::absl},
        {"fph", &BenchSet::fph},
        {"std", &BenchSet::std_map},
        {"flat", &BenchSet::flat},
    }};

    for (const auto& row_spec : kRows) {
        const auto& results = set.*(row_spec.member);
        std::vector<std::string> row;
        row.reserve(table.headers.size());
        row.emplace_back(row_spec.name);
        for (const auto& result : results) {
            row.emplace_back(format_cell(result));
        }
        table.rows.emplace_back(std::move(row));
    }

    table.widths.assign(table.headers.size(), 0);
    for (size_t idx = 0; idx < table.headers.size(); idx++) {
        table.widths[idx] = std::max(table.widths[idx], table.headers[idx].size());
        for (const auto& row : table.rows) {
            table.widths[idx] = std::max(table.widths[idx], row[idx].size());
        }
        table.widths[idx] += 2; // left + right padding
    }

    return table;
}

size_t grid_width(std::span<const size_t> widths) {
    size_t width = 1;
    for (const auto column_width : widths) {
        width += column_width + 1;
    }
    return width;
}

std::string align_cell(std::string_view value, size_t width, bool right_align) {
    if (value.size() >= width) {
        return std::string(value);
    }
    const auto padding = width - value.size();
    if (right_align) {
        return std::string(padding, ' ') + std::string(value);
    }
    return std::string(value) + std::string(padding, ' ');
}

void print_rule(std::span<const size_t> widths) {
    std::string line;
    line.reserve(grid_width(widths));
    for (const auto width : widths) {
        line.push_back('+');
        line.append(width, '-');
    }
    line.push_back('+');
    std::println("{}", line);
}

void print_row(
    std::span<const size_t> widths,
    std::span<const uint8_t> right_align,
    std::span<const std::string> row) {
    std::string line;
    line.reserve(grid_width(widths));
    for (size_t idx = 0; idx < widths.size(); idx++) {
        const bool align_right = right_align[idx] != 0;
        line.push_back('|');
        line.push_back(' ');
        line += align_cell(row[idx], widths[idx] - 2, align_right);
        line.push_back(' ');
    }
    line.push_back('|');
    std::println("{}", line);
}

void print_table(const Table& table) {
    std::vector<uint8_t> right_align(table.headers.size(), 1);
    right_align.front() = 0;

    print_rule(std::span<const size_t>{table.widths});
    print_row(
        std::span<const size_t>{table.widths},
        std::span<const uint8_t>{right_align},
        std::span<const std::string>{table.headers});
    print_rule(std::span<const size_t>{table.widths});
    for (const auto& row : table.rows) {
        print_row(
            std::span<const size_t>{table.widths},
            std::span<const uint8_t>{right_align},
            std::span<const std::string>{row});
    }
    print_rule(std::span<const size_t>{table.widths});
}

void print_section(std::string_view title, std::span<const BenchSet> results) {
    std::vector<TableOutput> tables;
    tables.reserve(results.size());
    size_t max_width = 0;
    for (const auto& entry : results) {
        auto table = make_table(entry);
        max_width = std::max(max_width, grid_width(std::span<const size_t>{table.widths}));
        tables.push_back({entry.shift, std::move(table)});
    }

    std::println("{}", std::string(max_width, '-'));
    std::println();
    std::println("{:^{}}", title, max_width);

    for (const auto& entry : tables) {
        std::println();
        std::println(
            "{:^{}}", std::format("N = {} (1 << {})", 1ULL << entry.shift, entry.shift), max_width);
        print_table(entry.table);
        std::println();
    }
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

    print_section("Spare elements", std::span<const BenchSet>{random_results});
    print_section("Dense set of elements", std::span<const BenchSet>{dense_results});

    return 0;
}
