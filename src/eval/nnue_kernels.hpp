#pragma once
#include <cstdint>

namespace nnue::kernels {
void add_scalar(std::int16_t* destination, const std::int16_t* row, int sign);
void transform_scalar(const std::int16_t* input, std::uint8_t* output);
void dense_scalar(const std::uint8_t* input,
                  const std::int8_t* weights,
                  const std::int32_t* bias,
                  std::int32_t* output,
                  int in,
                  int out);
void activate_scalar(const std::int32_t* input, std::uint8_t* output);
void add_avx2(std::int16_t* destination, const std::int16_t* row, int sign);
void transform_avx2(const std::int16_t* input, std::uint8_t* output);
void dense_avx2(const std::uint8_t* input,
                const std::int8_t* weights,
                const std::int32_t* bias,
                std::int32_t* output,
                int in,
                int out);
void activate_avx2(const std::int32_t* input, std::uint8_t* output);
} // namespace nnue::kernels
