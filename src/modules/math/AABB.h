#pragma once

#include <glm/glm.hpp>

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    AABB() = default;

    AABB(const glm::vec3& min, const glm::vec3& max)
        : min(min), max(max) {}

    glm::vec3 getCenter() const {
        return (min + max) * 0.5f;
    }

    glm::vec3 getSize() const {
        return max - min;
    }
};
