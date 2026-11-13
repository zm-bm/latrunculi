#include "eval/nnue.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "nnue_embedded.hpp"

namespace nnue {
static_assert(static_limit < scores::tt_mate_bound);
namespace {
constexpr std::size_t prefix_size = 44;
constexpr std::size_t header_size = 52;
constexpr std::size_t payload_size = (inputs * hidden + hidden + 2 * hidden) * 2 + 8;

std::uint64_t read(std::span<const std::uint8_t> bytes, std::size_t offset, int count) {
    if (offset + count > bytes.size())
        throw std::invalid_argument("truncated NNUE network");
    std::uint64_t value = 0;
    for (int i = 0; i < count; ++i)
        value |= std::uint64_t(bytes[offset + i]) << (8 * i);
    return value;
}

std::int64_t magnitude(std::int16_t value) { return value < 0 ? -int(value) : int(value); }
} // namespace

Network decode(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != header_size + payload_size)
        throw std::invalid_argument("invalid NNUE payload length");
    if (std::string_view(reinterpret_cast<const char*>(bytes.data()), 8) != "LATNNUE1")
        throw std::invalid_argument("invalid NNUE magic");
    constexpr std::array expected = {1, inputs, hidden, 2, 1, hidden_scale, output_scale};
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (read(bytes, 8 + i * 4, 4) != std::uint64_t(expected[i]))
            throw std::invalid_argument("incompatible NNUE architecture or quantization");
    const auto calibration = read(bytes, 36, 4);
    if (calibration < 32 || calibration > 2048 || read(bytes, 40, 4) != payload_size)
        throw std::invalid_argument("invalid NNUE calibration or length");
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (i < prefix_size || i >= header_size)
            hash = (hash ^ bytes[i]) * 1099511628211ULL;
    if (hash != read(bytes, prefix_size, 8))
        throw std::invalid_argument("NNUE checksum mismatch");
    Network result{};
    result.calibration = int(calibration);
    std::size_t offset = header_size;
    auto next = [&]() {
        const auto value = std::bit_cast<std::int16_t>(std::uint16_t(read(bytes, offset, 2)));
        offset += 2;
        return value;
    };
    std::int64_t max_weight = 0, max_bias = 0, output_bound = 0;
    for (auto& row : result.weights)
        for (auto& value : row) {
            value = next();
            max_weight = std::max(max_weight, magnitude(value));
        }
    for (auto& value : result.bias) {
        value = next();
        max_bias = std::max(max_bias, magnitude(value));
    }
    for (auto& value : result.output) {
        value = next();
        output_bound += magnitude(value) * hidden_scale;
    }
    result.output_bias = std::bit_cast<std::int64_t>(read(bytes, offset, 8));
    // Avoid abs(INT64_MIN), addition overflow, and overflow of S*n in production.
    const auto allowed_bias = std::numeric_limits<std::int64_t>::max() / result.calibration
        - output_bound;
    if (result.output_bias < -allowed_bias || result.output_bias > allowed_bias
        || max_bias + 64 * max_weight > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("unsafe NNUE arithmetic bounds");
    return result;
}

const Network& network() {
    static const Network weights = decode(embedded::bytes);
    return weights;
}

const char* network_sha256() { return embedded::sha256; }

void Accumulators::reset() noexcept {
    for (auto& perspective : values)
        std::copy(network().bias.begin(), network().bias.end(), perspective.begin());
}

void Accumulators::update(Color color, PieceType piece, Square square, int sign) noexcept {
    for (Color perspective : {BLACK, WHITE}) {
        const auto& weights = network().weights[feature(perspective, color, piece, square)];
        for (int i = 0; i < hidden; ++i)
            values[perspective][i] += sign * std::int32_t(weights[i]);
    }
}

void Accumulators::move(Color color, PieceType piece, Square from, Square to) noexcept {
    for (Color perspective : {BLACK, WHITE}) {
        const auto& removed = network().weights[feature(perspective, color, piece, from)];
        const auto& added = network().weights[feature(perspective, color, piece, to)];
        for (int i = 0; i < hidden; ++i)
            values[perspective][i] += std::int32_t(added[i]) - std::int32_t(removed[i]);
    }
}

Accumulators refresh(const Board& board) {
    Accumulators state;
    state.reset();
    for (Square square = A1; square != INVALID; ++square) {
        const Piece piece = board.piece_on(square);
        if (piece != NO_PIECE)
            state.update(color_of(piece), type_of(piece), square, 1);
    }
    return state;
}

Score evaluate(const Network& weights, const Accumulators& state, Color turn) noexcept {
    std::int64_t sum = weights.output_bias;
    for (int half = 0; half < 2; ++half) {
        const auto& values = state.values[half == 0 ? turn : ~turn];
        for (int i = 0; i < hidden; ++i)
            sum += std::int64_t(weights.output[half * hidden + i])
                * std::clamp(values[i], 0, hidden_scale);
    }
    // Signed C++ division truncates toward zero; decode proved S*sum cannot overflow.
    return Score(std::clamp(weights.calibration * sum / (hidden_scale * output_scale),
                            -std::int64_t(static_limit), std::int64_t(static_limit)));
}

Score evaluate(const Accumulators& state, Color turn) noexcept {
    return evaluate(network(), state, turn);
}
} // namespace nnue
