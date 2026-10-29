#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <type_traits>

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color{1.0f};
};

static_assert(std::is_standard_layout_v<Vertex>);
static_assert(sizeof(Vertex) == 64);

static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, normal) == 12);
static_assert(offsetof(Vertex, uv) == 24);
static_assert(offsetof(Vertex, tangent) == 32);
static_assert(offsetof(Vertex, color) == 48);
