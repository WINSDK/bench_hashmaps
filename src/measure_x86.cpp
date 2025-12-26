#include <x86intrin.h>
#include <cstdint>

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <array>
#include <cstring>

#include "measure.hpp"

uint64_t read_tsc() {
    _mm_lfence();
    uint64_t tsc = __rdtsc();
    _mm_lfence();
    return tsc;
}

namespace {
int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return static_cast<int>(syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags));
}

struct PerfEventGroup {
    std::array<int, 4> fds{-1, -1, -1, -1};
    bool ok = false;

    PerfEventGroup() {
        perf_event_attr leader{};
        leader.type = PERF_TYPE_HARDWARE;
        leader.size = sizeof(leader);
        leader.disabled = 0;
        leader.exclude_kernel = 1;
        leader.exclude_hv = 1;
        leader.read_format = PERF_FORMAT_GROUP;
        leader.config = PERF_COUNT_HW_CPU_CYCLES;
        fds[0] = perf_event_open(&leader, 0, -1, -1, 0);
        if (fds[0] < 0) {
            return;
        }

        auto open_member = [&](uint64_t config) {
            perf_event_attr attr{};
            attr.type = PERF_TYPE_HARDWARE;
            attr.size = sizeof(attr);
            attr.disabled = 0;
            attr.exclude_kernel = 1;
            attr.exclude_hv = 1;
            attr.config = config;
            return perf_event_open(&attr, 0, -1, fds[0], 0);
        };

        fds[1] = open_member(PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
        fds[2] = open_member(PERF_COUNT_HW_BRANCH_MISSES);
        fds[3] = open_member(PERF_COUNT_HW_INSTRUCTIONS);
        if (fds[1] < 0 || fds[2] < 0 || fds[3] < 0) {
            for (auto& fd : fds) {
                if (fd >= 0) {
                    close(fd);
                }
                fd = -1;
            }
            return;
        }
        ok = true;
    }

    ~PerfEventGroup() {
        for (auto fd : fds) {
            if (fd != -1) {
                close(fd);
                fd = -1;
            }
        }
    }

    PerfCounters read() const {
        if (!ok) {
            return PerfCounters{read_tsc(), 0, 0, 0};
        }

        struct ReadData {
            uint64_t nr;
            uint64_t values[4];
        };

        ReadData data{};
        const auto expected = static_cast<ssize_t>(sizeof(data));
        const auto res = ::read(fds[0], &data, sizeof(data));
        if (res < expected || data.nr < 4) {
            return PerfCounters{read_tsc(), 0, 0, 0};
        }
        return PerfCounters{data.values[0], data.values[1], data.values[2], data.values[3]};
    }
};
} // namespace

struct X86Recorder {
    PerfCounters get_counters() const {
        static PerfEventGroup group{};
        return group.read();
    }
};

PerfCounters PerfRecorder::get_counters() const {
    static X86Recorder instance{};
    return instance.get_counters();
}

PerfRecorder RECORDER{};
