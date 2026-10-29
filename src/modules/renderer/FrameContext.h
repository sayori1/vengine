#pragma once

#include "sokol_gfx.h"
#include <glm/mat4x4.hpp>
#include <cstdint>
#include <optional>

struct SceneColor {
    sg_swapchain target{};
};

struct Depth {
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
    bool hasCamera = false;
    sg_swapchain swapchain{};
    std::optional<SceneColor> sceneColor;
    std::optional<Depth> depth;
};
