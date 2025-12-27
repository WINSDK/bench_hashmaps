#pragma once

#include <cstdint>
#include <format>

struct PerfCounters {
    uint64_t cycles = 0;
    uint64_t branches = 0;
    uint64_t missed_branches = 0;
    uint64_t instructions = 0;
    uint64_t l1d_accesses = 0;
    uint64_t l1d_misses = 0;
    uint64_t llc_accesses = 0;
    uint64_t llc_misses = 0;
    uint64_t core_time_enabled = 0;
    uint64_t core_time_running = 0;
    uint64_t l1d_time_enabled = 0;
    uint64_t l1d_time_running = 0;
    uint64_t llc_time_enabled = 0;
    uint64_t llc_time_running = 0;

    explicit PerfCounters() {}
    explicit PerfCounters(
        uint64_t c,
        uint64_t b,
        uint64_t m,
        uint64_t i,
        uint64_t l1d_a = 0,
        uint64_t l1d_m = 0,
        uint64_t llc_a = 0,
        uint64_t llc_m = 0)
        : cycles(c),
          branches(b),
          missed_branches(m),
          instructions(i),
          l1d_accesses(l1d_a),
          l1d_misses(l1d_m),
          llc_accesses(llc_a),
          llc_misses(llc_m) {}

    PerfCounters& operator-=(const PerfCounters& other) {
        cycles -= other.cycles;
        branches -= other.branches;
        missed_branches -= other.missed_branches;
        instructions -= other.instructions;
        l1d_accesses -= other.l1d_accesses;
        l1d_misses -= other.l1d_misses;
        llc_accesses -= other.llc_accesses;
        llc_misses -= other.llc_misses;
        core_time_enabled -= other.core_time_enabled;
        core_time_running -= other.core_time_running;
        l1d_time_enabled -= other.l1d_time_enabled;
        l1d_time_running -= other.l1d_time_running;
        llc_time_enabled -= other.llc_time_enabled;
        llc_time_running -= other.llc_time_running;
        return *this;
    }
    PerfCounters operator-(const PerfCounters& other) const {
        PerfCounters result{};
        result.cycles = cycles - other.cycles;
        result.branches = branches - other.branches;
        result.missed_branches = missed_branches - other.missed_branches;
        result.instructions = instructions - other.instructions;
        result.l1d_accesses = l1d_accesses - other.l1d_accesses;
        result.l1d_misses = l1d_misses - other.l1d_misses;
        result.llc_accesses = llc_accesses - other.llc_accesses;
        result.llc_misses = llc_misses - other.llc_misses;
        result.core_time_enabled = core_time_enabled - other.core_time_enabled;
        result.core_time_running = core_time_running - other.core_time_running;
        result.l1d_time_enabled = l1d_time_enabled - other.l1d_time_enabled;
        result.l1d_time_running = l1d_time_running - other.l1d_time_running;
        result.llc_time_enabled = llc_time_enabled - other.llc_time_enabled;
        result.llc_time_running = llc_time_running - other.llc_time_running;
        return result;
    }
    PerfCounters& min(const PerfCounters& other) {
        cycles = other.cycles < cycles ? other.cycles : cycles;
        branches = other.branches < branches ? other.branches : branches;
        missed_branches =
            other.missed_branches < missed_branches ? other.missed_branches : missed_branches;
        instructions = other.instructions < instructions ? other.instructions : instructions;
        l1d_accesses = other.l1d_accesses < l1d_accesses ? other.l1d_accesses : l1d_accesses;
        l1d_misses = other.l1d_misses < l1d_misses ? other.l1d_misses : l1d_misses;
        llc_accesses = other.llc_accesses < llc_accesses ? other.llc_accesses : llc_accesses;
        llc_misses = other.llc_misses < llc_misses ? other.llc_misses : llc_misses;
        core_time_enabled =
            other.core_time_enabled < core_time_enabled ? other.core_time_enabled
                                                        : core_time_enabled;
        core_time_running =
            other.core_time_running < core_time_running ? other.core_time_running
                                                        : core_time_running;
        l1d_time_enabled =
            other.l1d_time_enabled < l1d_time_enabled ? other.l1d_time_enabled
                                                      : l1d_time_enabled;
        l1d_time_running =
            other.l1d_time_running < l1d_time_running ? other.l1d_time_running
                                                      : l1d_time_running;
        llc_time_enabled =
            other.llc_time_enabled < llc_time_enabled ? other.llc_time_enabled
                                                      : llc_time_enabled;
        llc_time_running =
            other.llc_time_running < llc_time_running ? other.llc_time_running
                                                      : llc_time_running;
        return *this;
    }
    PerfCounters& operator+=(const PerfCounters& other) {
        cycles += other.cycles;
        branches += other.branches;
        missed_branches += other.missed_branches;
        instructions += other.instructions;
        l1d_accesses += other.l1d_accesses;
        l1d_misses += other.l1d_misses;
        llc_accesses += other.llc_accesses;
        llc_misses += other.llc_misses;
        core_time_enabled += other.core_time_enabled;
        core_time_running += other.core_time_running;
        l1d_time_enabled += other.l1d_time_enabled;
        l1d_time_running += other.l1d_time_running;
        llc_time_enabled += other.llc_time_enabled;
        llc_time_running += other.llc_time_running;
        return *this;
    }

    PerfCounters& operator/=(uint64_t numerator) {
        const auto d = static_cast<double>(numerator);
        cycles = static_cast<uint64_t>(static_cast<double>(cycles) / d);
        branches = static_cast<uint64_t>(static_cast<double>(branches) / d);
        missed_branches = static_cast<uint64_t>(static_cast<double>(missed_branches) / d);
        instructions = static_cast<uint64_t>(static_cast<double>(instructions) / d);
        l1d_accesses = static_cast<uint64_t>(static_cast<double>(l1d_accesses) / d);
        l1d_misses = static_cast<uint64_t>(static_cast<double>(l1d_misses) / d);
        llc_accesses = static_cast<uint64_t>(static_cast<double>(llc_accesses) / d);
        llc_misses = static_cast<uint64_t>(static_cast<double>(llc_misses) / d);
        core_time_enabled = static_cast<uint64_t>(static_cast<double>(core_time_enabled) / d);
        core_time_running = static_cast<uint64_t>(static_cast<double>(core_time_running) / d);
        l1d_time_enabled = static_cast<uint64_t>(static_cast<double>(l1d_time_enabled) / d);
        l1d_time_running = static_cast<uint64_t>(static_cast<double>(l1d_time_running) / d);
        llc_time_enabled = static_cast<uint64_t>(static_cast<double>(llc_time_enabled) / d);
        llc_time_running = static_cast<uint64_t>(static_cast<double>(llc_time_running) / d);
        return *this;
    }

    PerfCounters operator/(uint64_t numerator) const {
        PerfCounters tmp = *this;
        tmp /= numerator;
        return tmp;
    }
};

template <>
struct std::formatter<PerfCounters> : std::formatter<uint64_t> {
    using Base = std::formatter<uint64_t>;

    auto format(const PerfCounters& pc, auto& ctx) const {
        auto out = ctx.out();

        out = std::format_to(out, "PerfCounters{{cycles=");
        ctx.advance_to(out);
        out = Base::format(pc.cycles, ctx);

        out = std::format_to(out, ", branches=");
        ctx.advance_to(out);
        out = Base::format(pc.branches, ctx);

        out = std::format_to(out, ", missed_branches=");
        ctx.advance_to(out);
        out = Base::format(pc.missed_branches, ctx);

        double hit_rate = 1.0;
        if (pc.branches > 0) {
            hit_rate =
                1.0 - (static_cast<double>(pc.missed_branches) / static_cast<double>(pc.branches));
        }

        out = std::format_to(out, ", hit_rate=");
        ctx.advance_to(out);
        out = Base::format(static_cast<uint64_t>(hit_rate * 100.0), ctx);

        out = std::format_to(out, "%, instructions=");
        ctx.advance_to(out);
        out = Base::format(pc.instructions, ctx);

        out = std::format_to(out, ", l1d_accesses=");
        ctx.advance_to(out);
        out = Base::format(pc.l1d_accesses, ctx);

        out = std::format_to(out, ", l1d_misses=");
        ctx.advance_to(out);
        out = Base::format(pc.l1d_misses, ctx);

        out = std::format_to(out, ", llc_accesses=");
        ctx.advance_to(out);
        out = Base::format(pc.llc_accesses, ctx);

        out = std::format_to(out, ", llc_misses=");
        ctx.advance_to(out);
        out = Base::format(pc.llc_misses, ctx);

        out = std::format_to(out, "}}");
        return out;
    }
};

enum class PerfCounterSet : uint8_t {
    core,
    cache,
};

struct PerfRecorder {
    PerfCounters get_counters(PerfCounterSet set) const;
    void enable(PerfCounterSet set) const;
    void disable_all() const;
};

extern PerfRecorder RECORDER;
