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
inline uint64_t __rdtsc() {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}
#else
#error "Unsupported arch"
#endif

inline void prefetch(const void* ptr) {
    __builtin_prefetch(ptr, 0, 0);
}
inline void* __aligned_alloc(size_t alignment, size_t size) {
    return std::aligned_alloc(alignment, size);
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
