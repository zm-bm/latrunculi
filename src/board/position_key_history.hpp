#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "core/types.hpp"

// Previous position keys for repetition detection.
struct PositionKeyHistory {
    static constexpr std::size_t capacity = 256;

    std::array<PositionKey, capacity> keys{};
    int                               size{};

    void clear() { size = 0; }

    void push(PositionKey key) {
        assert(size >= 0 && static_cast<std::size_t>(size) < keys.size());
        keys[size++] = key;
    }

    void pop(PositionKey expected_key) {
        assert(size > 0);
        assert(keys[size - 1] == expected_key);
        --size;
    }

    PositionKey operator[](int index) const {
        assert(index >= 0 && index < size);
        return keys[static_cast<std::size_t>(index)];
    }

    int count() const { return size; }
};
