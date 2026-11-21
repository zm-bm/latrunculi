#include "eval/nnue_network.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#if LATRUNCULI_EMBED_NETWORK
extern "C" {
extern const std::uint8_t latrunculi_nnue_start[];
extern const std::uint64_t latrunculi_nnue_size;
}
#endif

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

std::filesystem::path network_path() {
    // Resolve the executable itself, including when launched through PATH or a symlink.
    // The supported deployment target is Linux; the current directory is irrelevant.
    return std::filesystem::read_symlink("/proc/self/exe").parent_path() / network_filename;
}

Network load_network(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("cannot open HalfKP network: " + path.string());
    if (file.tellg() != std::streampos(file_size))
        throw std::invalid_argument("invalid HalfKP network length: " + path.string());
    std::vector<std::uint8_t> bytes(file_size);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        throw std::runtime_error("cannot read HalfKP network: " + path.string());
    return decode(bytes);
}

const Network& network() {
#if LATRUNCULI_EMBED_NETWORK
    static const Network value = decode({latrunculi_nnue_start, latrunculi_nnue_size});
#else
    static const Network value = load_network(network_path());
#endif
    return value;
}

} // namespace nnue
