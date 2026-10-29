#pragma once

#include "sokol_gfx.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>

struct Depth {
    sg_view attachment{};
    sg_view texture{};
};

struct ClusteredLights {
    sg_view lights{};
    sg_view clusters{};
    sg_view indices{};
    glm::uvec4 grid{1};
    glm::vec4 zParameters{0.1f, 100.0f, 1.0f, 0.0f};
    uint32_t lightCount = 0;
    uint32_t maxLightsPerCluster = 0;
};

struct Shadows {
    sg_view atlas{};
    sg_sampler sampler{};
    sg_view matrices{};
    glm::vec2 texelSize{1.0f};
};

struct SceneColor {
    sg_view texture{};
    sg_swapchain target{};
};

struct FrameContext {
    uint64_t frameIndex = 0;
    struct Viewport {
        int width = 0;
        int height = 0;
    } viewport;
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 viewProjection{1.0f};
    glm::mat4 inverseView{1.0f};
    glm::mat4 inverseProjection{1.0f};
    glm::vec3 cameraPosition{0.0f};
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool hasCamera = false;
    sg_swapchain swapchain{};
    std::optional<Depth> depth;
    std::optional<ClusteredLights> clusteredLights;
    std::optional<Shadows> shadows;
    std::optional<SceneColor> sceneColor;
};
