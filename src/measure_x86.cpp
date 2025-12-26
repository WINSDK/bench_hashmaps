#include <cstdint>
#include <x86intrin.h>

#include "measure.hpp"

uint64_t read_tsc() {
    _mm_lfence();
    uint64_t tsc = __rdtsc();
    _mm_lfence();
    return tsc;
}

struct X86Recorder {
    PerfCounters get_counters() const {
        return PerfCounters{read_tsc(), 0, 0, 0};
    }
};

PerfCounters PerfRecorder::get_counters() const {
    static X86Recorder instance{};
    return instance.get_counters();
}

PerfRecorder RECORDER{};
