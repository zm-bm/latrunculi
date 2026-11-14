#include "eval/nnue.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>

#include "board/board.hpp"
#include "eval/nnue_kernels.hpp"

extern "C" const std::uint8_t latrunculi_nnue_start[21022697];

namespace nnue {
namespace {
constexpr std::size_t file_size = 21022697;
constexpr std::array<std::uint32_t, 64> sha_constants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::string sha256(std::span<const std::uint8_t> bytes) {
    std::array<std::uint32_t, 8> hash = {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19};
    const std::size_t padded = ((bytes.size() + 9 + 63) / 64) * 64;
    for (std::size_t offset = 0; offset < padded; offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (int i = 0; i < 64; ++i) {
            const auto index = offset + i;
            std::uint8_t byte = 0;
            if (index < bytes.size())
                byte = bytes[index];
            else if (index == bytes.size())
                byte = 0x80;
            else if (index >= padded - 8)
                byte =
                    std::uint8_t((std::uint64_t(bytes.size()) * 8) >> ((padded - 1 - index) * 8));
            words[i / 4] |= std::uint32_t(byte) << (24 - (i % 4) * 8);
        }
        for (int i = 16; i < 64; ++i) {
            const auto a = words[i - 15], b = words[i - 2];
            words[i] = words[i - 16]
                + (std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3))
                + words[i - 7]
                + (std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10));
        }
        auto [a, b, c, d, e, f, g, h] = hash;
        for (int i = 0; i < 64; ++i) {
            const auto t1 = h
                + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25))
                + ((e & f) ^ (~e & g))
                + sha_constants[i]
                + words[i];
            const auto t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22))
                + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        const std::array block = {a, b, c, d, e, f, g, h};
        for (int i = 0; i < 8; ++i)
            hash[i] += block[i];
    }
    std::string result;
    for (auto word : hash)
        for (int shift = 28; shift >= 0; shift -= 4)
            result += "0123456789abcdef"[(word >> shift) & 15];
    return result;
}

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes)
        : bytes(bytes) {}
    std::uint32_t read(int size) {
        if (offset + size > bytes.size())
            throw std::invalid_argument("truncated HalfKP network");
        std::uint32_t value = 0;
        for (int i = 0; i < size; ++i)
            value |= std::uint32_t(bytes[offset++]) << (8 * i);
        return value;
    }
    void expect(std::uint32_t value) {
        if (read(4) != value)
            throw std::invalid_argument("incompatible HalfKP architecture");
    }
    void skip(std::size_t size) {
        if (offset + size > bytes.size())
            throw std::invalid_argument("truncated HalfKP description");
        offset += size;
    }
    std::size_t offset = 0;

private:
    std::span<const std::uint8_t> bytes;
};

template <int In, int Out>
void read_layer(Reader& reader, Layer<In, Out>& layer) {
    for (auto& v : layer.bias)
        v = std::bit_cast<std::int32_t>(reader.read(4));
    for (auto& v : layer.weights)
        v = std::bit_cast<std::int8_t>(std::uint8_t(reader.read(1)));
    for (int i = 0; i < Out; ++i) {
        std::int64_t bound = std::abs(std::int64_t(layer.bias[i]));
        for (int j = 0; j < In; ++j)
            bound += 127 * std::abs(int(layer.weights[i * In + j]));
        if (bound > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("unsafe HalfKP affine arithmetic");
    }
}
} // namespace

Network decode(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != file_size)
        throw std::invalid_argument("invalid HalfKP network length");
    Reader r(bytes);
    r.expect(0x7af32f16);
    r.expect(0x3e5aa6ee);
    r.skip(r.read(4));
    r.expect(0x5d69d7b8);
    Network n;
    for (auto& v : n.bias)
        v = std::bit_cast<std::int16_t>(std::uint16_t(r.read(2)));
    n.weights.resize(inputs * hidden);
    for (auto& v : n.weights)
        v = std::bit_cast<std::int16_t>(std::uint16_t(r.read(2)));
    int max_bias = 0, max_weight = 0;
    for (auto v : n.bias)
        max_bias = std::max(max_bias, std::abs(int(v)));
    for (auto v : n.weights)
        max_weight = std::max(max_weight, std::abs(int(v)));
    if (max_bias + 30 * max_weight > std::numeric_limits<std::int16_t>::max())
        throw std::invalid_argument("unsafe HalfKP accumulator arithmetic");
    r.expect(0x63337156);
    read_layer(r, n.first);
    read_layer(r, n.second);
    read_layer(r, n.output);
    if (r.offset != bytes.size() || sha256(bytes) != network_sha256)
        throw std::invalid_argument("frozen HalfKP network SHA256 mismatch");
    return n;
}

std::span<const std::uint8_t> embedded_network() {
    return latrunculi_nnue_start;
}

const Network& network() {
    static const Network value = decode(embedded_network());
    return value;
}

bool avx2_available() noexcept {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

Backend resolve_backend(BackendPreference preference, bool supported) {
    switch (preference) {
        case BackendPreference::Scalar: return Backend::Scalar;
        case BackendPreference::Auto:   return supported ? Backend::Avx2 : Backend::Scalar;
        case BackendPreference::Avx2:
            if (!supported)
                throw std::runtime_error("HalfKP AVX2 backend unsupported on this CPU");
            return Backend::Avx2;
    }
    throw std::invalid_argument("invalid HalfKP backend preference");
}

Backend backend() {
    static const Backend selected =
        resolve_backend(static_cast<BackendPreference>(LATRUNCULI_NNUE_BACKEND), avx2_available());
    return selected;
}
const char* backend_name() {
    return backend() == Backend::Avx2 ? "AVX2" : "SCALAR";
}

namespace kernels {
void add_scalar(std::int16_t* destination, const std::int16_t* row, int sign) {
    for (int i = 0; i < hidden; ++i)
        destination[i] = std::int16_t(destination[i] + sign * int(row[i]));
}
void transform_scalar(const std::int16_t* input, std::uint8_t* output) {
    for (int i = 0; i < hidden; ++i)
        output[i] = std::uint8_t(std::clamp(int(input[i]), 0, 127));
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
    for (int i = 0; i < 32; ++i)
        output[i] = std::uint8_t(std::min(127, std::max(0, input[i]) / 64));
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
          512,
          32);
    activate(result.first_sums.data(), result.first.data());
    dense(result.first.data(),
          n.second.weights.data(),
          n.second.bias.data(),
          result.second_sums.data(),
          32,
          32);
    activate(result.second_sums.data(), result.second.data());
    dense(result.second.data(),
          n.output.weights.data(),
          n.output.bias.data(),
          &result.numerator,
          32,
          1);
    result.raw = result.numerator / 16;
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
