#pragma once

#include <glm/mat4x4.hpp>
#include <memory>

class RenderContext;

class DepthRenderer {
public:
    DepthRenderer(const char* vertexSource, const char* fragmentSource, const char* label);
    ~DepthRenderer();
    void draw(RenderContext& render, const glm::mat4& viewProjection);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
