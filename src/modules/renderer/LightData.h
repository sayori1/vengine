#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <limits>

enum class LightType : uint32_t {
    Directional,
    Point,
    Spot
};

struct LightData {
    LightType type = LightType::Point;
    glm::mat4 transform{1.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    float innerAngle = 20.0f;
    float outerAngle = 30.0f;
    bool shadowsEnabled = false;
};

struct alignas(16) GPULight {
    glm::vec4 positionRadius{0.0f};
    glm::vec4 directionOuter{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 colorIntensity{1.0f};
    glm::vec4 parameters{0.0f};
    glm::uvec4 metadata{0, std::numeric_limits<uint32_t>::max(), 0, 0};
};

struct alignas(16) GPUShadow {
    glm::mat4 viewProjection{1.0f};
    glm::vec4 atlasRect{0.0f};
    glm::vec4 parameters{0.0f};
};

static_assert(sizeof(GPULight) == 80);
static_assert(sizeof(GPUShadow) == 96);
