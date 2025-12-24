#pragma once

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <print>

#if defined(__APPLE__)
constexpr auto CACHE_LINE = 128;
#else
constexpr auto CACHE_LINE = 64;
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <intrin.h>
#elif defined(__aarch64__)
#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
#else
#error "Unsupported arch"
#endif

inline uint64_t read_ticks() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
    const auto ticks = __rdtsc();
    _mm_lfence();
    return ticks;
#elif defined(__aarch64__) && defined(__APPLE__)
    return mach_absolute_time();
#elif defined(__aarch64__)
    uint64_t val;
    asm volatile("isb; mrs %0, cntvct_el0; isb" : "=r"(val) :: "memory");
    return val;
#endif
}

inline uint64_t tick_frequency_hz() {
#if defined(__aarch64__) && defined(__APPLE__)
    static uint64_t freq = []() {
        mach_timebase_info_data_t info{};
        mach_timebase_info(&info);
        return (1000000000ULL * info.denom) / info.numer;
    }();
    return freq;
#elif defined(__aarch64__)
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

inline void prefetch(const void* ptr) {
    __builtin_prefetch(ptr, 0, 0);
}
inline void* __aligned_alloc(size_t alignment, size_t size) {
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
#if defined(__APPLE__)
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, aligned_size) != 0) {
        return nullptr;
    }
    return ptr;
#else
    return std::aligned_alloc(alignment, aligned_size);
#endif
}
inline void __aligned_free(void* ptr) {
    return std::free(ptr);
}

// These constants are all large primes.
inline uint64_t squirrel3(uint64_t at) {
    constexpr uint64_t BIT_NOISE1 = 0x9E3779B185EBCA87ULL;
    constexpr uint64_t BIT_NOISE2 = 0xC2B2AE3D27D4EB4FULL;
    constexpr uint64_t BIT_NOISE3 = 0x27D4EB2F165667C5ULL;
    at *= BIT_NOISE1;
    at ^= (at >> 8);
    at += BIT_NOISE2;
    at ^= (at << 8);
    at *= BIT_NOISE3;
    at ^= (at >> 8);
    return at;
}
