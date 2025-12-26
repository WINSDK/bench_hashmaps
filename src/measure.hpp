#pragma once

#include <format>

struct PerfCounters {
    uint64_t cycles = 0;
    uint64_t branches = 0;
    uint64_t missed_branches = 0;
    uint64_t instructions = 0;
    uint64_t l1d_accesses = 0;
    uint64_t l1d_misses = 0;

    explicit PerfCounters() {}
    explicit PerfCounters(
        uint64_t c,
        uint64_t b,
        uint64_t m,
        uint64_t i,
        uint64_t l1d_a = 0,
        uint64_t l1d_m = 0)
        : cycles(c),
          branches(b),
          missed_branches(m),
          instructions(i),
          l1d_accesses(l1d_a),
          l1d_misses(l1d_m) {}

    PerfCounters& operator-=(const PerfCounters& other) {
        cycles -= other.cycles;
        branches -= other.branches;
        missed_branches -= other.missed_branches;
        instructions -= other.instructions;
        l1d_accesses -= other.l1d_accesses;
        l1d_misses -= other.l1d_misses;
        return *this;
    }
    PerfCounters operator-(const PerfCounters& other) const {
        return PerfCounters(
            cycles - other.cycles,
            branches - other.branches,
            missed_branches - other.missed_branches,
            instructions - other.instructions,
            l1d_accesses - other.l1d_accesses,
            l1d_misses - other.l1d_misses);
    }
    PerfCounters& min(const PerfCounters& other) {
        cycles = other.cycles < cycles ? other.cycles : cycles;
        branches = other.branches < branches ? other.branches : branches;
        missed_branches =
            other.missed_branches < missed_branches ? other.missed_branches : missed_branches;
        instructions = other.instructions < instructions ? other.instructions : instructions;
        l1d_accesses = other.l1d_accesses < l1d_accesses ? other.l1d_accesses : l1d_accesses;
        l1d_misses = other.l1d_misses < l1d_misses ? other.l1d_misses : l1d_misses;
        return *this;
    }
    PerfCounters& operator+=(const PerfCounters& other) {
        cycles += other.cycles;
        branches += other.branches;
        missed_branches += other.missed_branches;
        instructions += other.instructions;
        l1d_accesses += other.l1d_accesses;
        l1d_misses += other.l1d_misses;
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

        out = std::format_to(out, "}}");
        return out;
    }
};

struct PerfRecorder {
    PerfCounters get_counters() const;
};

extern PerfRecorder RECORDER;
