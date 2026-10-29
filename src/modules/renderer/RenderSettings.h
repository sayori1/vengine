#pragma once

#include <glm/glm.hpp>
#include <cstdint>

struct RenderSettings {
    glm::uvec3 clusterGrid{16, 9, 24};
    uint32_t maxLights = 256;
    uint32_t maxLightsPerCluster = 64;
    uint32_t maxShadowLights = 8;
    int shadowResolution = 512;
    uint32_t cascadeCount = 4;
    float shadowDistance = 80.0f;
    float shadowBias = 0.001f;
    float shadowNormalBias = 0.02f;
    float cascadeSplitLambda = 0.65f;
    float exposure = 1.0f;
    glm::vec3 ambientColor{0.03f};
};
