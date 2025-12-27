#include <x86intrin.h>
#include <cstdint>

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <print>
#include <string_view>

#include "measure.hpp"

uint64_t read_tsc() {
    _mm_lfence();
    uint64_t tsc = __rdtsc();
    _mm_lfence();
    return tsc;
}

int perf_event_open(perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    long res = syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    return static_cast<int>(res);
}

struct PerfEventGroup {
    static constexpr size_t EVENT_COUNT_CORE = 4;
    std::array<int, EVENT_COUNT_CORE> fds{};
    bool ok = false;

    PerfEventGroup() {
        fds.fill(-1);
        ok = open_group();
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

        std::array<uint64_t, 3 + EVENT_COUNT_CORE> data{};
        const size_t expected_words = 3 + EVENT_COUNT_CORE;
        const size_t expected_bytes = expected_words * sizeof(uint64_t);
        const auto res = ::read(fds[0], data.data(), data.size() * sizeof(uint64_t));
        if (res < static_cast<ssize_t>(expected_bytes) || data[0] < EVENT_COUNT_CORE) {
            return PerfCounters{read_tsc(), 0, 0, 0};
        }

        PerfCounters counters{};
        counters.cycles = data[3];
        counters.branches = data[4];
        counters.missed_branches = data[5];
        counters.instructions = data[6];
        counters.core_time_enabled = data[1];
        counters.core_time_running = data[2];

        return counters;
    }

private:
    bool open_group() {
        perf_event_attr leader{};
        leader.type = PERF_TYPE_HARDWARE;
        leader.size = sizeof(leader);
        leader.disabled = 0;
        leader.exclude_kernel = 1;
        leader.exclude_hv = 1;
        leader.read_format =
            PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
        leader.config = PERF_COUNT_HW_CPU_CYCLES;
        fds[0] = perf_event_open(&leader, 0, -1, -1, 0);
        if (fds[0] < 0) {
            std::println(
                stderr,
                "perf: perf_event_open failed for cpu-cycles: {} (errno={})",
                std::strerror(errno),
                errno);
            return false;
        }

        auto open_member_hw = [&](uint64_t config, std::string_view name) {
            perf_event_attr attr{};
            attr.type = PERF_TYPE_HARDWARE;
            attr.size = sizeof(attr);
            attr.disabled = 0;
            attr.exclude_kernel = 1;
            attr.exclude_hv = 1;
            attr.config = config;
            const auto fd = perf_event_open(&attr, 0, -1, fds[0], 0);
            if (fd < 0) {
                std::println(
                    stderr,
                    "perf: perf_event_open failed for {}: {} (errno={})",
                    name,
                    std::strerror(errno),
                    errno);
            }
            return fd;
        };

        fds[1] = open_member_hw(PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "branch-instructions");
        fds[2] = open_member_hw(PERF_COUNT_HW_BRANCH_MISSES, "branch-misses");
        fds[3] = open_member_hw(PERF_COUNT_HW_INSTRUCTIONS, "instructions");
        if (fds[1] < 0 || fds[2] < 0 || fds[3] < 0) {
            close_all();
            return false;
        }

        ok = true;
        return true;
    }

    void close_all() {
        for (auto& fd : fds) {
            if (fd != -1) {
                close(fd);
            }
            fd = -1;
        }
        ok = false;
    }
};

struct CacheCounters {
    uint64_t accesses = 0;
    uint64_t misses = 0;
    uint64_t time_enabled = 0;
    uint64_t time_running = 0;
};

struct L1dEventGroup {
    static constexpr size_t EVENT_COUNT = 2;
    std::array<int, EVENT_COUNT> fds{};
    bool ok = false;

    L1dEventGroup() {
        fds.fill(-1);
        if (!open_group()) {
            std::println(stderr, "perf: L1D events unavailable");
        }
    }

    ~L1dEventGroup() {
        for (auto fd : fds) {
            if (fd != -1) {
                close(fd);
                fd = -1;
            }
        }
    }

    CacheCounters read() const {
        if (!ok) {
            return {};
        }

        std::array<uint64_t, 3 + EVENT_COUNT> data{};
        const size_t expected_bytes = data.size() * sizeof(uint64_t);
        const auto res = ::read(fds[0], data.data(), expected_bytes);
        if (res < static_cast<ssize_t>(expected_bytes) || data[0] < EVENT_COUNT) {
            return {};
        }

        CacheCounters counters{};
        counters.accesses = data[3];
        counters.misses = data[4];
        counters.time_enabled = data[1];
        counters.time_running = data[2];
        return counters;
    }

private:
    bool open_group() {
        auto open_member_cache =
            [&](uint64_t cache_id, uint64_t op, uint64_t result, std::string_view name) {
                perf_event_attr attr{};
                attr.type = PERF_TYPE_HW_CACHE;
                attr.size = sizeof(attr);
                attr.disabled = 0;
                attr.exclude_kernel = 1;
                attr.exclude_hv = 1;
                attr.config = cache_id | (op << 8) | (result << 16);
                const auto fd = perf_event_open(&attr, 0, -1, fds[0], 0);
                if (fd < 0) {
                    std::println(
                        stderr,
                        "perf: perf_event_open failed for {}: {} (errno={})",
                        name,
                        std::strerror(errno),
                        errno);
                }
                return fd;
            };

        perf_event_attr leader{};
        leader.type = PERF_TYPE_HW_CACHE;
        leader.size = sizeof(leader);
        leader.disabled = 0;
        leader.exclude_kernel = 1;
        leader.exclude_hv = 1;
        leader.read_format =
            PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
        leader.config = PERF_COUNT_HW_CACHE_L1D |
            (PERF_COUNT_HW_CACHE_OP_READ << 8) |
            (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
        fds[0] = perf_event_open(&leader, 0, -1, -1, 0);
        if (fds[0] < 0) {
            std::println(
                stderr,
                "perf: perf_event_open failed for l1d-read-access: {} (errno={})",
                std::strerror(errno),
                errno);
            close_all();
            return false;
        }

        fds[1] = open_member_cache(
            PERF_COUNT_HW_CACHE_L1D,
            PERF_COUNT_HW_CACHE_OP_READ,
            PERF_COUNT_HW_CACHE_RESULT_MISS,
            "l1d-read-miss");
        if (fds[1] < 0) {
            close_all();
            return false;
        }

        ok = true;
        return true;
    }

    void close_all() {
        for (auto& fd : fds) {
            if (fd != -1) {
                close(fd);
            }
            fd = -1;
        }
        ok = false;
    }
};

struct LlcEventGroup {
    static constexpr size_t EVENT_COUNT = 2;
    std::array<int, EVENT_COUNT> fds{};
    bool ok = false;

    LlcEventGroup() {
        fds.fill(-1);
        if (!open_group()) {
            std::println(stderr, "perf: LLC events unavailable");
        }
    }

    ~LlcEventGroup() {
        for (auto fd : fds) {
            if (fd != -1) {
                close(fd);
                fd = -1;
            }
        }
    }

    CacheCounters read() const {
        if (!ok) {
            return {};
        }

        std::array<uint64_t, 3 + EVENT_COUNT> data{};
        const size_t expected_bytes = data.size() * sizeof(uint64_t);
        const auto res = ::read(fds[0], data.data(), expected_bytes);
        if (res < static_cast<ssize_t>(expected_bytes) || data[0] < EVENT_COUNT) {
            return {};
        }

        CacheCounters counters{};
        counters.accesses = data[3];
        counters.misses = data[4];
        counters.time_enabled = data[1];
        counters.time_running = data[2];
        return counters;
    }

private:
    bool open_group() {
        auto open_member_cache =
            [&](uint64_t cache_id, uint64_t op, uint64_t result, std::string_view name) {
                perf_event_attr attr{};
                attr.type = PERF_TYPE_HW_CACHE;
                attr.size = sizeof(attr);
                attr.disabled = 0;
                attr.exclude_kernel = 1;
                attr.exclude_hv = 1;
                attr.config = cache_id | (op << 8) | (result << 16);
                const auto fd = perf_event_open(&attr, 0, -1, fds[0], 0);
                if (fd < 0) {
                    std::println(
                        stderr,
                        "perf: perf_event_open failed for {}: {} (errno={})",
                        name,
                        std::strerror(errno),
                        errno);
                }
                return fd;
            };

        perf_event_attr leader{};
        leader.type = PERF_TYPE_HW_CACHE;
        leader.size = sizeof(leader);
        leader.disabled = 0;
        leader.exclude_kernel = 1;
        leader.exclude_hv = 1;
        leader.read_format =
            PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
        leader.config = PERF_COUNT_HW_CACHE_LL |
            (PERF_COUNT_HW_CACHE_OP_READ << 8) |
            (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
        fds[0] = perf_event_open(&leader, 0, -1, -1, 0);
        if (fds[0] < 0) {
            std::println(
                stderr,
                "perf: perf_event_open failed for llc-read-access: {} (errno={})",
                std::strerror(errno),
                errno);
            close_all();
            return false;
        }

        fds[1] = open_member_cache(
            PERF_COUNT_HW_CACHE_LL,
            PERF_COUNT_HW_CACHE_OP_READ,
            PERF_COUNT_HW_CACHE_RESULT_MISS,
            "llc-read-miss");
        if (fds[1] < 0) {
            close_all();
            return false;
        }

        ok = true;
        return true;
    }

    void close_all() {
        for (auto& fd : fds) {
            if (fd != -1) {
                close(fd);
            }
            fd = -1;
        }
        ok = false;
    }
};

struct X86Recorder {
    PerfCounters get_counters() const {
        static PerfEventGroup group{};
        static L1dEventGroup l1d_group{};
        static LlcEventGroup llc_group{};
        auto counters = group.read();
        const auto l1d = l1d_group.read();
        const auto llc = llc_group.read();
        counters.l1d_accesses = l1d.accesses;
        counters.l1d_misses = l1d.misses;
        counters.l1d_time_enabled = l1d.time_enabled;
        counters.l1d_time_running = l1d.time_running;
        counters.llc_accesses = llc.accesses;
        counters.llc_misses = llc.misses;
        counters.llc_time_enabled = llc.time_enabled;
        counters.llc_time_running = llc.time_running;
        return counters;
    }
};

PerfCounters PerfRecorder::get_counters() const {
    static X86Recorder instance{};
    return instance.get_counters();
}

PerfRecorder RECORDER{};
