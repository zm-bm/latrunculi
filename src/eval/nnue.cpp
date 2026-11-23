#include "eval/nnue.hpp"

#include <algorithm>
#include <cassert>

#include "board/board.hpp"
#include "eval/nnue_kernels.hpp"
#include "eval/nnue_network.hpp"

namespace nnue {
bool avx2_available() noexcept {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

Backend resolve_backend(bool supported) noexcept {
    return supported ? Backend::Avx2 : Backend::Scalar;
}

Backend backend() {
    static const Backend selected = resolve_backend(avx2_available());
    return selected;
}
const char* backend_name() {
    return backend() == Backend::Avx2 ? "AVX2" : "SCALAR";
}

void initialize() {
    (void)backend();
    (void)network();
}

namespace kernels {
void add_scalar(std::int16_t* destination, const std::int16_t* row, int sign) {
    for (int i = 0; i < hidden; ++i)
        destination[i] = std::int16_t(destination[i] + sign * int(row[i]));
}
void transform_scalar(const std::int16_t* input, std::uint8_t* output) {
    for (int i = 0; i < hidden; ++i)
        output[i] = std::uint8_t(std::clamp(int(input[i]), 0, activation_max));
}
void dense_scalar(const std::uint8_t* input,
                  const std::int8_t* weights,
                  const std::int32_t* bias,
                  std::int32_t* output,
                  int in,
                  int out) {
    for (int i = 0; i < out; ++i) {
        std::int32_t sum = bias[i];
        for (int j = 0; j < in; ++j)
            sum += int(input[j]) * int(weights[i * in + j]);
        output[i] = sum;
    }
}
void activate_scalar(const std::int32_t* input, std::uint8_t* output) {
    for (int i = 0; i < layer_width; ++i)
        output[i] = std::uint8_t(std::min(activation_max, std::max(0, input[i]) / activation_scale));
}
} // namespace kernels

void refresh(const Board& board, Accumulators& state, Color perspective, Backend mode) {
    state.values[perspective] = network().bias;
    const auto king = board.king_sq(perspective);
    const auto add = mode == Backend::Scalar ? kernels::add_scalar : kernels::add_avx2;
    for (Square square = A1; square != INVALID; ++square) {
        const auto piece = board.piece_on(square);
        if (piece == NO_PIECE || type_of(piece) == KING)
            continue;
        const int index = feature(perspective, king, color_of(piece), type_of(piece), square);
        add(state.values[perspective].data(), network().weights.data() + index * hidden, 1);
    }
}
Accumulators refresh(const Board& board, Backend mode) {
    Accumulators result;
    refresh(board, result, WHITE, mode);
    refresh(board, result, BLACK, mode);
    return result;
}
Accumulators refresh(const Board& board) {
    return refresh(board, backend());
}

void update(Accumulators& state,
            Color perspective,
            Square king,
            Color color,
            PieceType piece,
            Square square,
            int sign) {
    assert(piece != KING && piece != NO_PIECETYPE && (sign == 1 || sign == -1));
    const auto row =
        network().weights.data() + feature(perspective, king, color, piece, square) * hidden;
    (backend() == Backend::Scalar ? kernels::add_scalar
                                  : kernels::add_avx2)(state.values[perspective].data(), row, sign);
}

Trace trace(const Network& n, const Accumulators& state, Color turn, Backend mode) {
    Trace result;
    const bool scalar = mode == Backend::Scalar;
    const auto transform = scalar ? kernels::transform_scalar : kernels::transform_avx2;
    const auto dense = scalar ? kernels::dense_scalar : kernels::dense_avx2;
    const auto activate = scalar ? kernels::activate_scalar : kernels::activate_avx2;
    transform(state.values[turn].data(), result.transformed.data());
    transform(state.values[~turn].data(), result.transformed.data() + hidden);
    dense(result.transformed.data(),
          n.first.weights.data(),
          n.first.bias.data(),
          result.first_sums.data(),
          transformed_size,
          layer_width);
    activate(result.first_sums.data(), result.first.data());
    dense(result.first.data(),
          n.second.weights.data(),
          n.second.bias.data(),
          result.second_sums.data(),
          layer_width,
          layer_width);
    activate(result.second_sums.data(), result.second.data());
    dense(result.second.data(),
          n.output.weights.data(),
          n.output.bias.data(),
          &result.numerator,
          layer_width,
          1);
    result.raw = result.numerator / output_divisor;
    return result;
}
int raw(const Accumulators& state, Color turn) {
    return trace(network(), state, turn, backend()).raw;
}
Score adapt(int value) noexcept {
    const auto neural = std::int64_t(value) * 5 / 4 + 28;
    return Score(
        std::clamp(neural * 202 / 208, -std::int64_t(static_limit), std::int64_t(static_limit)));
}
Score evaluate(const Accumulators& state, Color turn) {
    return adapt(raw(state, turn));
}
} // namespace nnue
