#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

// Previous position keys for repetition detection.
struct PositionKeyHistory {
    static constexpr std::size_t capacity = 256;

    std::array<uint64_t, capacity> keys{};
    uint16_t                              size{};

    void clear() { size = 0; }

    void push(uint64_t key) {
        assert(size < keys.size());
        keys[size++] = key;
    }

    void pop(uint64_t expected_key) {
        assert(size > 0);
        assert(keys[size - 1] == expected_key);
        --size;
    }

    uint64_t operator[](int index) const {
        assert(index >= 0 && index < size);
        return keys[index];
    }

    int count() const { return size; }
};
