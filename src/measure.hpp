#pragma once

#include <format>

struct PerfCounters {
    double cycles;
    double branches;
    double missed_branches;
    double instructions;
    PerfCounters(uint64_t c, uint64_t b, uint64_t m, uint64_t i)
        : cycles(c), branches(b), missed_branches(m), instructions(i) {}
    PerfCounters(double c, double b, double m, double i)
        : cycles(c), branches(b), missed_branches(m), instructions(i) {}
    PerfCounters(double init)
        : cycles(init), branches(init), missed_branches(init), instructions(init) {}

    inline PerfCounters& operator-=(const PerfCounters& other) {
        cycles -= other.cycles;
        branches -= other.branches;
        missed_branches -= other.missed_branches;
        instructions -= other.instructions;
        return *this;
    }
    inline PerfCounters operator-(const PerfCounters& other) const {
        return PerfCounters(
            cycles - other.cycles,
            branches - other.branches,
            missed_branches - other.missed_branches,
            instructions - other.instructions);
    }
    inline PerfCounters& min(const PerfCounters& other) {
        cycles = other.cycles < cycles ? other.cycles : cycles;
        branches = other.branches < branches ? other.branches : branches;
        missed_branches =
            other.missed_branches < missed_branches ? other.missed_branches : missed_branches;
        instructions = other.instructions < instructions ? other.instructions : instructions;
        return *this;
    }
    inline PerfCounters& operator+=(const PerfCounters& other) {
        cycles += other.cycles;
        branches += other.branches;
        missed_branches += other.missed_branches;
        instructions += other.instructions;
        return *this;
    }

    inline PerfCounters& operator/=(double numerator) {
        cycles /= numerator;
        branches /= numerator;
        missed_branches /= numerator;
        instructions /= numerator;
        return *this;
    }
};

template <>
struct std::formatter<PerfCounters> : std::formatter<double> {
    using Base = std::formatter<double>;

    auto format(const PerfCounters& pc, auto& ctx) const {
        auto out = ctx.out();

        out = std::format_to(out, "performance_counters{{cycles=");
        ctx.advance_to(out);
        out = Base::format(pc.cycles, ctx);

        out = std::format_to(out, ", branches=");
        ctx.advance_to(out);
        out = Base::format(pc.branches, ctx);

        out = std::format_to(out, ", missed_branches=");
        ctx.advance_to(out);
        out = Base::format(pc.missed_branches, ctx);

        out = std::format_to(out, ", instructions=");
        ctx.advance_to(out);
        out = Base::format(pc.instructions, ctx);

        out = std::format_to(out, "}}");
        return out;
    }
};

// The maximum number of counters we could read from every class in one go.
// ARMV7: FIXED: 1, CONFIGURABLE: 4
// ARM32: FIXED: 2, CONFIGURABLE: 6
// ARM64: FIXED: 2, CONFIGURABLE: CORE_NCTRS - FIXED (6 or 8)
// x86: 32
constexpr auto KPC_MAX_COUNTERS = 32;

class EventRecorder {
    uint64_t regs[KPC_MAX_COUNTERS] = {0};
    size_t counter_map[KPC_MAX_COUNTERS] = {0};
    uint64_t counters_0[KPC_MAX_COUNTERS] = {0};
    uint64_t counters_1[KPC_MAX_COUNTERS] = {0};

public:
    explicit EventRecorder();

    PerfCounters get_counters();
};

extern EventRecorder RECORDER;
