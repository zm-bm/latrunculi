#include "eval/nnue_kernels.hpp"
#if defined(__x86_64__)
#include <cassert>
#include <immintrin.h>

namespace nnue::kernels {
// Keep AVX2 confined to these functions; the dispatcher and scalar code remain generic.
__attribute__((target("avx2"))) void add_avx2(
    std::int16_t* destination,
    const std::int16_t* row,
    int sign) {
    for (int i = 0; i < 256; i += 16) {
        const auto a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(destination + i));
        const auto b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(destination + i),
                            sign == 1 ? _mm256_add_epi16(a, b) : _mm256_sub_epi16(a, b));
    }
}
__attribute__((target("avx2"))) void transform_avx2(const std::int16_t* input,
                                                    std::uint8_t* output) {
    const auto zero = _mm256_setzero_si256(), ceiling = _mm256_set1_epi16(127);
    for (int i = 0; i < 256; i += 16) {
        auto v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i));
        v = _mm256_min_epi16(_mm256_max_epi16(v, zero), ceiling);
        const auto bytes =
            _mm_packus_epi16(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(output + i), bytes);
    }
}
__attribute__((target("avx2"))) void dense_avx2(
    const std::uint8_t* input,
    const std::int8_t* weights,
    const std::int32_t* bias,
    std::int32_t* output,
    int in,
    int out) {
    assert(in % 32 == 0);
    const auto ones = _mm256_set1_epi16(1);
    for (int i = 0; i < out; ++i) {
        auto sum = _mm256_setzero_si256();
        for (int j = 0; j < in; j += 32) {
            const auto a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + j));
            const auto b =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + i * in + j));
            // Inputs are in [0,127]; each signed pair is within [-32512,32258].
            // Thus maddubs' signed-16 saturation cannot change the dot product.
            const auto pairs = _mm256_maddubs_epi16(a, b);
            sum = _mm256_add_epi32(sum, _mm256_madd_epi16(pairs, ones));
        }
        auto reduced = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
        reduced = _mm_hadd_epi32(reduced, reduced);
        reduced = _mm_hadd_epi32(reduced, reduced);
        output[i] = bias[i] + _mm_cvtsi128_si32(reduced);
    }
}
__attribute__((target("avx2"))) void activate_avx2(const std::int32_t* input,
                                                   std::uint8_t* output) {
    const auto zero = _mm256_setzero_si256(), ceiling = _mm256_set1_epi32(127);
    for (int i = 0; i < 32; i += 8) {
        auto v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i));
        v = _mm256_min_epi32(_mm256_max_epi32(_mm256_srai_epi32(v, 6), zero), ceiling);
        const auto words =
            _mm_packs_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
        const auto bytes = _mm_packus_epi16(words, _mm_setzero_si128());
        _mm_storel_epi64(reinterpret_cast<__m128i*>(output + i), bytes);
    }
}
} // namespace nnue::kernels
#else
#include <stdexcept>
namespace nnue::kernels {
void add_avx2(std::int16_t*, const std::int16_t*, int) {
    throw std::logic_error("AVX2 unsupported");
}
void transform_avx2(const std::int16_t*, std::uint8_t*) {
    throw std::logic_error("AVX2 unsupported");
}
void dense_avx2(const std::uint8_t*,
                const std::int8_t*,
                const std::int32_t*,
                std::int32_t*,
                int,
                int) {
    throw std::logic_error("AVX2 unsupported");
}
void activate_avx2(const std::int32_t*, std::uint8_t*) {
    throw std::logic_error("AVX2 unsupported");
}
} // namespace nnue::kernels
#endif
