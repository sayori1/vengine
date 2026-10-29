#pragma once

#include <cstdint>
#include <limits>

struct RID {
    static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

    uint32_t index = INVALID_INDEX;
    uint32_t generation = 0;

    constexpr RID() = default;

    constexpr RID(uint32_t index, uint32_t generation)
        : index(index), generation(generation) {}

    constexpr bool isValid() const {
        return index != INVALID_INDEX && generation != 0;
    }

    constexpr bool operator==(const RID&) const = default;
    constexpr bool operator!=(const RID&) const = default;
};
